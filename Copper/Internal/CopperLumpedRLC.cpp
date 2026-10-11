#include "CopperLumpedRLC.hpp"

#include <cmath>

#include "CSPrimBox.h"
#include "CSPropLumpedElement.h"

namespace copper {

namespace {

// Ported from Operator_Ext_LumpedRLC::BuildExtension()'s SERIES branch
// (openEMS/FDTD/extensions/operator_ext_lumpedRLC.cpp:264-297,361-400) -- see this file's
// header comment for why this is a clean-room reimplementation rather than a read of that
// extension's own (protected) state.
std::vector<CopperLumpedRLCCell> _discoverForProperty(CSPropLumpedElement& prop, const CopperYeeGrid& grid,
                                                        CopperOperator& op) {
    std::vector<CopperLumpedRLCCell> cells;
    if (prop.GetLEtype() != CSPropLumpedElement::SERIES) {
        return cells;
    }
    const int dir = prop.GetDirection();
    if (dir < 0 || dir > 2) {
        return cells;
    }
    const int dirP1 = (dir + 1) % 3;
    const int dirP2 = (dir + 2) % 3;

    const double rawR = prop.GetResistance();
    const double rawL = prop.GetInductance();
    const double rawC = prop.GetCapacity();
    // NaN means "not physically present" (see LumpedComponentConfig's own doc comment); a negative
    // value is nonsensical and clamped to absent too, matching BuildExtension()'s own clamping
    // (just without reproducing its warning text -- this is a clean-room GPU-side path, the CPU
    // backend's own vendor extension already warns about this same board via the real
    // CSPropLumpedElement it shares).
    const double R = (std::isnan(rawR) || rawR < 0.0) ? 0.0 : rawR;
    const double L = (std::isnan(rawL) || rawL < 0.0) ? 0.0 : rawL;
    const bool hasC = !std::isnan(rawC) && rawC > 0.0;
    const double C = hasC ? rawC : 0.0;

    const double dT = op.timestepSeconds();

    for (std::size_t p = 0; p < prop.GetQtyPrimitives(); ++p) {
        CSPrimBox* box = prop.GetPrimitive(p)->ToBox();
        if (box == nullptr) {
            continue;
        }
        double dstart[3];
        double dstop[3];
        for (int n = 0; n < 3; ++n) {
            dstart[n] = box->GetCoord(2 * n);
            dstop[n] = box->GetCoord(2 * n + 1);
        }
        unsigned int uiStart[3];
        unsigned int uiStop[3];
        const int snapDim = op.snapBox2Mesh(dstart, dstop, uiStart, uiStop, /*dualMesh=*/false, /*snapMethod=*/0);
        if (snapDim <= 0) {
            continue; // outside the domain, or a degenerate box -- nothing to do
        }
        if (uiStart[dir] == uiStop[dir]) {
            continue; // zero length along the current-carrying axis -- invalid, skip
        }

        // Only edges that aren't already metal carry the element. A box drawn pad centre to pad
        // centre lies largely on its pads' copper, whose PEC edges are skipped below; dividing the
        // value over every edge of the box regardless left a 62-ohm resistor across a gap a
        // quarter of its length at about 15 ohms. So count, per transverse column, the edges that
        // remain: each column carries the element's full value in series along those edges, and
        // the columns that have any share it in parallel.
        const auto isActive = [&](const unsigned int at[3]) {
            return grid.vi[dir][copperGridIndex(grid.dims, at[0], at[1], at[2])] != 0.0F;
        };
        unsigned int nParActive = 0;
        unsigned int pos[3] = {0, 0, 0};
        for (pos[dirP1] = uiStart[dirP1]; pos[dirP1] <= uiStop[dirP1]; ++pos[dirP1]) {
            for (pos[dirP2] = uiStart[dirP2]; pos[dirP2] <= uiStop[dirP2]; ++pos[dirP2]) {
                for (pos[dir] = uiStart[dir]; pos[dir] < uiStop[dir]; ++pos[dir]) {
                    if (isActive(pos)) {
                        ++nParActive;
                        break;
                    }
                }
            }
        }
        if (nParActive == 0) {
            continue; // the whole box is metal -- nothing left to carry the element
        }

        for (pos[dirP1] = uiStart[dirP1]; pos[dirP1] <= uiStop[dirP1]; ++pos[dirP1]) {
            for (pos[dirP2] = uiStart[dirP2]; pos[dirP2] <= uiStop[dirP2]; ++pos[dirP2]) {
                unsigned int nActive = 0;
                for (pos[dir] = uiStart[dir]; pos[dir] < uiStop[dir]; ++pos[dir]) {
                    if (isActive(pos)) ++nActive;
                }
                if (nActive == 0) continue;

                const double dL = L * static_cast<double>(nParActive) / static_cast<double>(nActive);
                const double dR = R * static_cast<double>(nParActive) / static_cast<double>(nActive);
                const double dC = hasC ? C * static_cast<double>(nActive) / static_cast<double>(nParActive) : 0.0;

                double ib0 = 0.0;
                double b1 = 0.0;
                double b2 = 0.0;
                if (dC == 0.0) {
                    ib0 = dT / (2.0 * dL + dT * dR);
                    b1 = -4.0 * dL / dT;
                    b2 = (2.0 * dL - dT * dR) / dT;
                } else {
                    ib0 = 2.0 * dT * dC / (4.0 * dL * dC + 2.0 * dT * dR * dC + dT * dT);
                    b1 = (dT * dT - 4.0 * dL * dC) / (dT * dC);
                    b2 = (4.0 * dL * dC - 2.0 * dT * dR * dC + dT * dT) / (2.0 * dT * dC);
                }
                // A SERIES element with no R, L, or C at all (dC==0 branch, dL==0, dR==0) makes ib0 =
                // dT/0 = inf -- mathematically undefined for this ADE formulation, not just
                // numerically unlucky (openEMS's own real Operator_Ext_LumpedRLC has the identical
                // division and no guard against it either: IsLElumpedRLC() accepts every SERIES-type
                // element regardless of its R/L/C values). A "lumped element" with no impedance at all
                // isn't a real RLC in the FDTD sense -- whatever conductive geometry it represents is
                // already captured as ordinary copper/PEC elsewhere -- so skip it here rather than
                // injecting a NaN/Inf coefficient that corrupts the field on the very first
                // applyLumpedRLC() call of the run.
                if (!std::isfinite(ib0) || !std::isfinite(b1) || !std::isfinite(b2)) {
                    continue;
                }

                for (pos[dir] = uiStart[dir]; pos[dir] < uiStop[dir]; ++pos[dir]) {
                    const std::uint32_t idx = copperGridIndex(grid.dims, pos[0], pos[1], pos[2]);
                    // See this file's header comment: with this cell's conductance already zeroed by
                    // the real Operator_Ext_LumpedRLC (which still runs for the GPU backend too, as
                    // part of the shared SetupFDTD() call), vv=1 and vi=dT/Cd -- recovering Cd this
                    // way needs no vendor-internals access at all.
                    const double vi = static_cast<double>(grid.vi[dir][idx]);
                    if (vi == 0.0) {
                        continue; // metal (PEC) edge: already counted out of nActive above
                    }
                    const double cd = dT / vi;

                    CopperLumpedRLCCell cell;
                    cell.x = pos[0];
                    cell.y = pos[1];
                    cell.z = pos[2];
                    cell.axis = static_cast<std::uint32_t>(dir);
                    cell.ib0 = static_cast<float>(ib0);
                    cell.b1 = static_cast<float>(b1);
                    cell.b2 = static_cast<float>(b2);
                    cell.vv2 = static_cast<float>(0.5 * dT * ib0 / cd);
                    cell.vj1 = static_cast<float>(0.5 * dT * (b1 * ib0 - 1.0) / cd);
                    cell.vj2 = static_cast<float>(0.5 * dT * b2 * ib0 / cd);
                    cell.vvd = static_cast<float>(1.0 / (1.0 + 0.5 * dT * ib0 / cd));
                    cells.push_back(cell);
                }
            }
        }
    }
    return cells;
}

} // namespace

std::vector<CopperLumpedRLCCell> discoverLumpedRLC(ContinuousStructure& csx, const CopperYeeGrid& grid,
                                                     CopperOperator& op) {
    std::vector<CopperLumpedRLCCell> result;
    for (CSProperties* prop : csx.GetPropertyByType(CSProperties::LUMPED_ELEMENT)) {
        auto* lumped = dynamic_cast<CSPropLumpedElement*>(prop);
        if (lumped == nullptr) {
            continue;
        }
        std::vector<CopperLumpedRLCCell> cells = _discoverForProperty(*lumped, grid, op);
        result.insert(result.end(), cells.begin(), cells.end());
    }
    return result;
}

} // namespace copper
