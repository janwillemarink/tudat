#include "Tudat/Astrodynamics/ObservationModels/observationViabilityCalculator.h"

namespace tudat
{

namespace observation_models
{

bool isObservationViable(
        const std::vector< Eigen::Vector6d >& states, const std::vector< double >& times, const LinkEnds& linkEnds,
        const std::map< LinkEnds, std::vector< boost::shared_ptr< ObservationViabilityCalculator > > >& viabilityCalculators )
{
    bool isObservationFeasible = 1;

    if( viabilityCalculators.count( linkEnds ) > 0 )
    {
        isObservationFeasible = isObservationViable( states, times, viabilityCalculators.at( linkEnds ) );
    }

    return isObservationFeasible;
}

bool isObservationViable(
        const std::vector< Eigen::Vector6d >& states, const std::vector< double >& times,
        const std::vector< boost::shared_ptr< ObservationViabilityCalculator > >& viabilityCalculators )
{
    bool isObservationFeasible = 1;

    for( unsigned int i = 0; i < viabilityCalculators.size( ); i++ )
    {
        if( viabilityCalculators.at( i )->isObservationViable( states, times ) == 0 )
        {
            isObservationFeasible = 0;
            break;
        }
    }

    return isObservationFeasible;
}

//! Function for determining whether the elevation angle at station is sufficient to allow observation
bool MinimumElevationAngleCalculator::isObservationViable(
        const std::vector< Eigen::Vector6d >& linkEndStates,
        const std::vector< double >& linkEndTimes )
{
    bool isObservationPossible = 1;

    // Iterate over all sets of entries of input vector for which elvation angle is to be checked.
    for( unsigned int i = 0; i < linkEndIndices_.size( ); i++ )
    {    
        // Check if elevation angle criteria is met for current link.
        if( ground_stations::isTargetInView( linkEndTimes.at( linkEndIndices_.at( i ).first ),
                            ( linkEndStates.at( linkEndIndices_.at( i ).second ) - linkEndStates.at( linkEndIndices_.at( i ).first ) )
                            .segment( 0, 3 ), pointingAngleCalculator_, minimumElevationAngle_ ) == 0 )
        {
            isObservationPossible = 0;
        }
    }

    return isObservationPossible;
}





}

}