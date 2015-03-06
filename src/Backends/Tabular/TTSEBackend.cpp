#if !defined(NO_TABULAR_BACKENDS)


#include "TTSEBackend.h"
#include "CoolProp.h"

void CoolProp::TTSEBackend::update(CoolProp::input_pairs input_pair, double val1, double val2)
{
    // Flush the cached indices (set to large number)
    cached_single_phase_i = std::numeric_limits<std::size_t>::max(); 
    cached_single_phase_j = std::numeric_limits<std::size_t>::max();
    cached_saturation_iL = std::numeric_limits<std::size_t>::max(); 
    cached_saturation_iV = std::numeric_limits<std::size_t>::max();
    
    switch(input_pair){
        case HmolarP_INPUTS:{
            _hmolar = val1; _p = val2;
            if (!single_phase_logph.native_inputs_are_in_range(_hmolar, _p)){
                // Use the AbstractState instance
                using_single_phase_table = false;
                if (get_debug_level() > 5){ std::cout << "inputs are not in range"; }
                throw ValueError(format("inputs are not in range, hmolar=%Lg, p=%Lg", static_cast<CoolPropDbl>(_hmolar), _p));
            }
            else{
                using_single_phase_table = true; // Use the table !
                std::size_t iL, iV;
                CoolPropDbl hL = 0, hV = 0;
                if (pure_saturation.is_inside(_p, iHmolar, _hmolar, iL, iV, hL, hV)){
                    using_single_phase_table = false;
                    _Q = (static_cast<double>(_hmolar)-hL)/(hV-hL);
                    if(!is_in_closed_range(0.0,1.0,static_cast<double>(_Q))){
                        throw ValueError("vapor quality is not in (0,1)");
                    }
                    else{
                        cached_saturation_iL = iL; cached_saturation_iV = iV;
                    }
                }
                else{
                    // Find and cache the indices i, j
                    selected_table = SELECTED_PH_TABLE;
                    single_phase_logph.find_native_nearest_neighbor(_hmolar, _p, cached_single_phase_i, cached_single_phase_j);
                }
            }
            break;
        }
        case HmassP_INPUTS:{
            update(HmolarP_INPUTS, val1 * AS->molar_mass(), val2); // H: [J/kg] * [kg/mol] -> [J/mol]
            return;
        }
        case DmolarP_INPUTS:
        case PUmolar_INPUTS:
        case PSmolar_INPUTS:
            throw ValueError("To be implemented as a 1-D iteration using PH table");
        case PT_INPUTS:{
            _p = val1; _T = val2;
            if (!single_phase_logpT.native_inputs_are_in_range(_T, _p)){
                // Use the AbstractState instance
                using_single_phase_table = false;
                if (get_debug_level() > 5){ std::cout << "inputs are not in range"; }
                throw ValueError(format("inputs are not in range, p=%Lg, T=%Lg", _p, _T));
            }
            else{
                using_single_phase_table = true; // Use the table !
                std::size_t iL = 0, iV = 0;
                CoolPropDbl TL = 0, TV = 0;
                if (pure_saturation.is_inside(_p, iT, _T, iL, iV, TL, TV)){
                    using_single_phase_table = false;
                    throw ValueError(format("P,T with TTSE cannot be two-phase for now"));
                }
                else{
                    // Find and cache the indices i, j
                    selected_table = SELECTED_PT_TABLE;
                    single_phase_logpT.find_native_nearest_neighbor(_T, _p, cached_single_phase_i, cached_single_phase_j);
                }
            }
            break;
        }
        case SmolarT_INPUTS:
        case DmolarT_INPUTS:
            throw ValueError("To be implemented as a 1-D iteration using PT table");
        default:
            throw ValueError();
    }
}

/// Use the single-phase table to evaluate an output
double CoolProp::TTSEBackend::evaluate_single_phase(SinglePhaseGriddedTableData &table, parameters output, double x, double y, std::size_t i, std::size_t j)
{
    // Define pointers for the matrices to be used; 
    std::vector<std::vector<double> > *z, *dzdx, *dzdy, *d2zdxdy, *d2zdy2, *d2zdx2;
    
    // Connect the pointers based on the output variable desired
    switch(output){
        case iT:
            z = &table.T; dzdx = &table.dTdx; dzdy = &table.dTdy;
            d2zdxdy = &table.d2Tdxdy; d2zdx2 = &table.d2Tdx2; d2zdy2 = &table.d2Tdy2;
            break;
        case iDmolar:
            z = &table.rhomolar; dzdx = &table.drhomolardx; dzdy = &table.drhomolardy;
            d2zdxdy = &table.d2rhomolardxdy; d2zdx2 = &table.d2rhomolardx2; d2zdy2 = &table.d2rhomolardy2;
            break;
        case iSmolar:
            z = &table.smolar; dzdx = &table.dsmolardx; dzdy = &table.dsmolardy;
            d2zdxdy = &table.d2smolardxdy; d2zdx2 = &table.d2smolardx2; d2zdy2 = &table.d2smolardy2;
            break;
        case iHmolar:
            z = &table.hmolar; dzdx = &table.dhmolardx; dzdy = &table.dhmolardy;
            d2zdxdy = &table.d2hmolardxdy; d2zdx2 = &table.d2hmolardx2; d2zdy2 = &table.d2hmolardy2;
            break;
        //case iUmolar:
        case iviscosity:
            z = &table.visc; break;
        case iconductivity:
            z = &table.cond; break;
        default:
            throw ValueError();
    }
    
    // Distances from the node
	double deltax = x - table.xvec[i];
    double deltay = y - table.yvec[j];
    
    if (output == iconductivity || output == iviscosity){
        // Linear interpolation
    }
    
    // Calculate the output value desired
    double val = (*z)[i][j]+deltax*(*dzdx)[i][j]+deltay*(*dzdy)[i][j]+0.5*deltax*deltax*(*d2zdx2)[i][j]+0.5*deltay*deltay*(*d2zdy2)[i][j]+deltay*deltax*(*d2zdxdy)[i][j];
    
    // Cache the output value calculated
    switch(output){
        case iT:  _T = val; break;
        case iDmolar: _rhomolar = val; break;
        case iSmolar: _smolar = val; break;
        case iHmolar: _hmolar = val; break;
        //case iUmolar:
        default: throw ValueError();
    }
    return val;
}

#endif // !defined(NO_TABULAR_BACKENDS)