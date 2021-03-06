/*    Copyright (c) 2010-2017, Delft University of Technology
 *    All rigths reserved
 *
 *    This file is part of the Tudat. Redistribution and use in source and
 *    binary forms, with or without modification, are permitted exclusively
 *    under the terms of the Modified BSD license. You should have received
 *    a copy of the license with this file. If not, please or visit:
 *    http://tudat.tudelft.nl/LICENSE.
 *
 */

#ifndef ORBITDETERMINATIONTESTCASES_H
#define ORBITDETERMINATIONTESTCASES_H

#include <boost/make_shared.hpp>

#include "Tudat/Astrodynamics/ObservationModels/simulateObservations.h"
#include "Tudat/Astrodynamics/OrbitDetermination/orbitDeterminationManager.h"
#include "Tudat/SimulationSetup/tudatSimulationHeader.h"
#include "Tudat/SimulationSetup/EnvironmentSetup/createGroundStations.h"

namespace tudat
{
namespace unit_tests
{

//Using declarations.
using namespace tudat::observation_models;
using namespace tudat::orbit_determination;
using namespace tudat::estimatable_parameters;
using namespace tudat::interpolators;
using namespace tudat::numerical_integrators;
using namespace tudat::spice_interface;
using namespace tudat::simulation_setup;
using namespace tudat::orbital_element_conversions;
using namespace tudat::ephemerides;
using namespace tudat::propagators;
using namespace tudat::basic_astrodynamics;
using namespace tudat::coordinate_conversions;


Eigen::VectorXd getDefaultInitialParameterPerturbation( )
{
    Eigen::VectorXd parameterPerturbations = Eigen::VectorXd( 7 );
    for( int i = 0; i < 3; i++ )
    {
        parameterPerturbations( i ) = 1.0E3;
        parameterPerturbations( i + 3 ) = 1.0E-2;
    }
    parameterPerturbations( 6 ) = 5.0E6;

    return parameterPerturbations;
}

template< typename TimeType = double, typename StateScalarType  = double >
std::pair< boost::shared_ptr< PodOutput< StateScalarType > >, Eigen::VectorXd > executePlanetaryParameterEstimation(
        const int observableType = 1,
        Eigen::VectorXd parameterPerturbation = getDefaultInitialParameterPerturbation( ),
        Eigen::MatrixXd inverseAPrioriCovariance  = Eigen::MatrixXd::Zero( 7, 7 ),
        const double weight = 1.0 )
{
    //Load spice kernels.
    spice_interface::loadStandardSpiceKernels( );

    //Define setting for total number of bodies and those which need to be integrated numerically.
    //The first numberOfNumericalBodies from the bodyNames vector will be integrated numerically.

    std::vector< std::string > bodyNames;
    bodyNames.push_back( "Earth" );
    bodyNames.push_back( "Mars" );
    bodyNames.push_back( "Sun" );
    bodyNames.push_back( "Moon" );
    bodyNames.push_back( "Jupiter" );
    bodyNames.push_back( "Saturn" );

    // Specify initial time
    TimeType initialEphemerisTime = TimeType( 1.0E7 );
    TimeType finalEphemerisTime = TimeType( 3.0E7 );
    double maximumTimeStep = 3600.0;

    double buffer = 10.0 * maximumTimeStep;

    std::map< std::string, boost::shared_ptr< BodySettings > > bodySettings =
            getDefaultBodySettings( bodyNames,initialEphemerisTime - buffer, finalEphemerisTime + buffer );
    bodySettings[ "Moon" ]->ephemerisSettings->resetFrameOrigin( "Sun" );

    // Create bodies needed in simulation
    NamedBodyMap bodyMap = createBodies( bodySettings );

    setGlobalFrameBodyEphemerides( bodyMap, "SSB", "ECLIPJ2000" );


    // Set accelerations between bodies that are to be taken into account.
    SelectedAccelerationMap accelerationMap;
    std::map< std::string, std::vector< boost::shared_ptr< AccelerationSettings > > > accelerationsOfEarth;
    accelerationsOfEarth[ "Sun" ].push_back( boost::make_shared< AccelerationSettings >( central_gravity ) );
    accelerationsOfEarth[ "Moon" ].push_back( boost::make_shared< AccelerationSettings >( central_gravity ) );
    accelerationsOfEarth[ "Mars" ].push_back( boost::make_shared< AccelerationSettings >( central_gravity ) );
    accelerationsOfEarth[ "Jupiter" ].push_back( boost::make_shared< AccelerationSettings >( central_gravity ) );
    accelerationsOfEarth[ "Saturn" ].push_back( boost::make_shared< AccelerationSettings >( central_gravity ) );

    accelerationMap[ "Earth" ] = accelerationsOfEarth;


    // Set bodies for which initial state is to be estimated and integrated.
    std::vector< std::string > bodiesToEstimate;
    bodiesToEstimate.push_back( "Earth" );
    std::vector< std::string > bodiesToIntegrate;
    bodiesToIntegrate.push_back( "Earth" );
    unsigned int numberOfNumericalBodies = bodiesToIntegrate.size( );

    // Define propagator settings.
    std::vector< std::string > centralBodies;
    std::map< std::string, std::string > centralBodyMap;

    centralBodies.resize( numberOfNumericalBodies );
    for( unsigned int i = 0; i < numberOfNumericalBodies; i++ )
    {
        centralBodies[ i ] = "SSB";
        centralBodyMap[ bodiesToIntegrate[ i ] ] = centralBodies[ i ];
    }

    AccelerationMap accelerationModelMap = createAccelerationModelsMap(
                bodyMap, accelerationMap, centralBodyMap );

    // Set parameters that are to be estimated.
    std::vector< boost::shared_ptr< EstimatableParameterSettings > > parameterNames;
    parameterNames.push_back( boost::make_shared< InitialTranslationalStateEstimatableParameterSettings< StateScalarType > >(
                                  "Earth", propagators::getInitialStateOfBody< TimeType, StateScalarType >(
                                      "Earth", centralBodyMap[ "Earth" ], bodyMap, initialEphemerisTime ),
                              centralBodyMap[ "Earth" ] ) );
    parameterNames.push_back( boost::make_shared< EstimatableParameterSettings >( "Moon", gravitational_parameter ) );

    boost::shared_ptr< estimatable_parameters::EstimatableParameterSet< StateScalarType > > parametersToEstimate =
            createParametersToEstimate< StateScalarType >( parameterNames, bodyMap );


    // Define integrator settings.
    boost::shared_ptr< IntegratorSettings< TimeType > > integratorSettings =
            boost::make_shared< IntegratorSettings< TimeType > >(
                rungeKutta4, TimeType( initialEphemerisTime - 4.0 * maximumTimeStep ), 900.0 );


    boost::shared_ptr< TranslationalStatePropagatorSettings< StateScalarType > > propagatorSettings =
            boost::make_shared< TranslationalStatePropagatorSettings< StateScalarType > >
            ( centralBodies, accelerationModelMap, bodiesToIntegrate,
              getInitialStateVectorOfBodiesToEstimate( parametersToEstimate ),
              TimeType( finalEphemerisTime + 4.0 * maximumTimeStep ),
              cowell, boost::shared_ptr< DependentVariableSaveSettings >( ) );


    // Define link ends
    LinkEnds linkEnds;
    observation_models::ObservationSettingsMap observationSettingsMap;

    if(observableType == 0 )
    {
        linkEnds[ observed_body ] = std::make_pair( "Earth", "" );
        observationSettingsMap.insert( std::make_pair( linkEnds, boost::make_shared< ObservationSettings >(
                                                           position_observable ) ) );
    }
    else
    {
        linkEnds[ transmitter ] = std::make_pair( "Earth", "" );
        linkEnds[ receiver ] = std::make_pair( "Mars", "" );

        if( observableType == 1 )
        {

            observationSettingsMap.insert( std::make_pair( linkEnds, boost::make_shared< ObservationSettings >(
                                                               one_way_range ) ) );
        }
        else if( observableType == 2 )
        {
            observationSettingsMap.insert( std::make_pair( linkEnds, boost::make_shared< ObservationSettings >(
                                                               angular_position ) ) );
        }
        else if( observableType == 3 )
        {
            observationSettingsMap.insert( std::make_pair( linkEnds, boost::make_shared< ObservationSettings >(
                                                               one_way_doppler ) ) );
        }
        else if( observableType == 4 )
        {
            observationSettingsMap.insert( std::make_pair( linkEnds, boost::make_shared< ObservationSettings >(
                                                               one_way_range ) ) );
            observationSettingsMap.insert( std::make_pair( linkEnds, boost::make_shared< ObservationSettings >(
                                                               one_way_doppler ) ) );
            observationSettingsMap.insert( std::make_pair( linkEnds, boost::make_shared< ObservationSettings >(
                                                               angular_position ) ) );
        }
    }



    // Create orbit determination object.
    OrbitDeterminationManager< StateScalarType, TimeType > orbitDeterminationManager =
            OrbitDeterminationManager< StateScalarType, TimeType >(
                bodyMap, parametersToEstimate, observationSettingsMap,
                integratorSettings, propagatorSettings );


    // Define observation times.
    double observationTimeStep = 1000.0;
    TimeType observationTime = Time( initialEphemerisTime + 10.0E4 );
    int numberOfObservations = 18000;

    std::vector< TimeType > initialObservationTimes;
    initialObservationTimes.resize( numberOfObservations );

    for( int i = 0; i < numberOfObservations; i++ )
    {
        initialObservationTimes[ i ] = observationTime;
        observationTime += observationTimeStep;
    }

    // Define observation simulation settings
    std::map< ObservableType, std::map< LinkEnds, std::pair< std::vector< TimeType >, LinkEndType > > > measurementSimulationInput;
    std::map< LinkEnds, std::pair< std::vector< TimeType >, LinkEndType > > singleObservableSimulationInput;
    initialObservationTimes = utilities::addScalarToVector( initialObservationTimes, 30.0 );
    if( observableType == 0 )
    {
        singleObservableSimulationInput[ linkEnds ] = std::make_pair( initialObservationTimes, observed_body );
        measurementSimulationInput[ position_observable ] = singleObservableSimulationInput;
    }
    else
    {
        singleObservableSimulationInput[ linkEnds ] = std::make_pair( initialObservationTimes, transmitter );

        if( observableType == 1 )
        {
            measurementSimulationInput[ one_way_range ] = singleObservableSimulationInput;
        }
        else if( observableType == 2 )
        {
            measurementSimulationInput[ angular_position ] = singleObservableSimulationInput;
        }
        else if( observableType == 3 )
        {
            measurementSimulationInput[ one_way_doppler ] = singleObservableSimulationInput;
        }
        else if( observableType == 4 )
        {
            measurementSimulationInput[ one_way_range ] = singleObservableSimulationInput;
            measurementSimulationInput[ one_way_doppler ] = singleObservableSimulationInput;
            measurementSimulationInput[ angular_position ] = singleObservableSimulationInput;
        }
    }

    singleObservableSimulationInput.clear( );

    typedef Eigen::Matrix< StateScalarType, Eigen::Dynamic, 1 > ObservationVectorType;
    typedef std::map< LinkEnds, std::pair< ObservationVectorType, std::pair< std::vector< TimeType >, LinkEndType > > > SingleObservablePodInputType;
    typedef std::map< ObservableType, SingleObservablePodInputType > PodInputDataType;

    // Simulate observations
    PodInputDataType observationsAndTimes = simulateObservations< StateScalarType, TimeType >(
                measurementSimulationInput, orbitDeterminationManager.getObservationSimulators( ) );

    // Perturb parameter estimate
    Eigen::Matrix< StateScalarType, Eigen::Dynamic, 1 > initialParameterEstimate =
            parametersToEstimate->template getFullParameterValues< StateScalarType >( );
    Eigen::Matrix< StateScalarType, Eigen::Dynamic, 1 > truthParameters = initialParameterEstimate;
    if( parameterPerturbation.rows( ) == 0 )
    {
        parameterPerturbation = Eigen::VectorXd::Zero( 7 );
    }
    for( unsigned int i = 0; i < 7; i++ )
    {
        initialParameterEstimate( i ) += parameterPerturbation( i );
    }

    // Define estimation input
    boost::shared_ptr< PodInput< StateScalarType, TimeType > > podInput =
            boost::make_shared< PodInput< StateScalarType, TimeType > >(
                observationsAndTimes, initialParameterEstimate.rows( ), inverseAPrioriCovariance,
                initialParameterEstimate - truthParameters );
    if( observableType == 4 )
    {
        std::map< observation_models::ObservableType, double > weightPerObservable;
        weightPerObservable[ one_way_range ] = 1.0 / ( 1.0 * 1.0 );
        weightPerObservable[ angular_position ] = 1.0 / ( 1.0E-9 * 1.0E-9 );
        weightPerObservable[ one_way_doppler ] = 1.0 / ( 1.0E-12 * 1.0E-12 );

        podInput->setConstantPerObservableWeightsMatrix( weightPerObservable );
    }
    else
    {
        podInput->setConstantWeightsMatrix( weight );
    }
    podInput->defineEstimationSettings( true, true, false, false, false );

    // Perform estimation
    boost::shared_ptr< PodOutput< StateScalarType > > podOutput = orbitDeterminationManager.estimateParameters(
                podInput, boost::make_shared< EstimationConvergenceChecker >( ) );

    return std::make_pair( podOutput,
                           ( podOutput->parameterEstimate_.template cast< double >( ) -
                             truthParameters .template cast< double >( ) ) );
}

template< typename TimeType = double, typename StateScalarType  = double >
Eigen::VectorXd executeEarthOrbiterParameterEstimation(
        std::pair< boost::shared_ptr< PodOutput< StateScalarType > >,
        boost::shared_ptr< PodInput< StateScalarType, TimeType > > >& podData,
        const TimeType startTime = TimeType( 1.0E7 ),
        const int numberOfDaysOfData = 3,
        const int numberOfIterations = 5,
        const bool useFullParameterSet = true )
{

    //Load spice kernels.
    spice_interface::loadStandardSpiceKernels( );

    // Define bodies in simulation
    std::vector< std::string > bodyNames;
    bodyNames.push_back( "Earth" );
    bodyNames.push_back( "Sun" );
    bodyNames.push_back( "Moon" );
    bodyNames.push_back( "Mars" );

    // Specify initial time
    TimeType initialEphemerisTime = startTime;
    TimeType finalEphemerisTime = initialEphemerisTime + numberOfDaysOfData * 86400.0;

    // Create bodies needed in simulation
    std::map< std::string, boost::shared_ptr< BodySettings > > bodySettings =
            getDefaultBodySettings( bodyNames );
    bodySettings[ "Earth" ]->rotationModelSettings = boost::make_shared< SimpleRotationModelSettings >(
                "ECLIPJ2000", "IAU_Earth",
                spice_interface::computeRotationQuaternionBetweenFrames(
                    "ECLIPJ2000", "IAU_Earth", initialEphemerisTime ),
                initialEphemerisTime, 2.0 * mathematical_constants::PI /
                ( physical_constants::JULIAN_DAY ) );

    NamedBodyMap bodyMap = createBodies( bodySettings );
    bodyMap[ "Vehicle" ] = boost::make_shared< Body >( );
    bodyMap[ "Vehicle" ]->setConstantBodyMass( 400.0 );

    // Create aerodynamic coefficient interface settings.
    double referenceArea = 4.0;
    double aerodynamicCoefficient = 1.2;
    boost::shared_ptr< AerodynamicCoefficientSettings > aerodynamicCoefficientSettings =
            boost::make_shared< ConstantAerodynamicCoefficientSettings >(
                referenceArea, aerodynamicCoefficient * ( Eigen::Vector3d( ) << 1.2, -0.1, -0.4 ).finished( ), 1, 1 );

    // Create and set aerodynamic coefficients object
    bodyMap[ "Vehicle" ]->setAerodynamicCoefficientInterface(
                createAerodynamicCoefficientInterface( aerodynamicCoefficientSettings, "Vehicle" ) );

    // Create radiation pressure settings
    double referenceAreaRadiation = 4.0;
    double radiationPressureCoefficient = 1.2;
    std::vector< std::string > occultingBodies;
    occultingBodies.push_back( "Earth" );
    boost::shared_ptr< RadiationPressureInterfaceSettings > asterixRadiationPressureSettings =
            boost::make_shared< CannonBallRadiationPressureInterfaceSettings >(
                "Sun", referenceAreaRadiation, radiationPressureCoefficient, occultingBodies );

    // Create and set radiation pressure settings
    bodyMap[ "Vehicle" ]->setRadiationPressureInterface(
                "Sun", createRadiationPressureInterface(
                    asterixRadiationPressureSettings, "Vehicle", bodyMap ) );

    bodyMap[ "Vehicle" ]->setEphemeris( boost::make_shared< TabulatedCartesianEphemeris< > >(
                                            boost::shared_ptr< interpolators::OneDimensionalInterpolator
                                            < double, Eigen::Vector6d > >( ), "Earth", "ECLIPJ2000" ) );

    setGlobalFrameBodyEphemerides( bodyMap, "Earth", "ECLIPJ2000" );


    // Creatre ground stations: same position, but different representation
    std::vector< std::string > groundStationNames;
    groundStationNames.push_back( "Station1" );
    groundStationNames.push_back( "Station2" );
    groundStationNames.push_back( "Station3" );

    createGroundStation( bodyMap.at( "Earth" ), "Station1", ( Eigen::Vector3d( ) << 0.0, 0.35, 0.0 ).finished( ), geodetic_position );
    createGroundStation( bodyMap.at( "Earth" ), "Station2", ( Eigen::Vector3d( ) << 0.0, -0.55, 2.0 ).finished( ), geodetic_position );
    createGroundStation( bodyMap.at( "Earth" ), "Station3", ( Eigen::Vector3d( ) << 0.0, 0.05, 4.0 ).finished( ), geodetic_position );

    // Set accelerations on Vehicle that are to be taken into account.
    SelectedAccelerationMap accelerationMap;
    std::map< std::string, std::vector< boost::shared_ptr< AccelerationSettings > > > accelerationsOfVehicle;
    accelerationsOfVehicle[ "Earth" ].push_back( boost::make_shared< SphericalHarmonicAccelerationSettings >( 8, 8 ) );
    accelerationsOfVehicle[ "Sun" ].push_back( boost::make_shared< AccelerationSettings >(
                                                   basic_astrodynamics::central_gravity ) );
    accelerationsOfVehicle[ "Moon" ].push_back( boost::make_shared< AccelerationSettings >(
                                                    basic_astrodynamics::central_gravity ) );
    accelerationsOfVehicle[ "Mars" ].push_back( boost::make_shared< AccelerationSettings >(
                                                    basic_astrodynamics::central_gravity ) );
    accelerationsOfVehicle[ "Sun" ].push_back( boost::make_shared< AccelerationSettings >(
                                                   basic_astrodynamics::cannon_ball_radiation_pressure ) );
    accelerationsOfVehicle[ "Earth" ].push_back( boost::make_shared< AccelerationSettings >(
                                                     basic_astrodynamics::aerodynamic ) );
    accelerationMap[ "Vehicle" ] = accelerationsOfVehicle;

    // Set bodies for which initial state is to be estimated and integrated.
    std::vector< std::string > bodiesToIntegrate;
    std::vector< std::string > centralBodies;
    bodiesToIntegrate.push_back( "Vehicle" );
    centralBodies.push_back( "Earth" );

    // Create acceleration models
    AccelerationMap accelerationModelMap = createAccelerationModelsMap(
                bodyMap, accelerationMap, bodiesToIntegrate, centralBodies );

    // Set Keplerian elements for Asterix.
    Eigen::Vector6d asterixInitialStateInKeplerianElements;
    asterixInitialStateInKeplerianElements( semiMajorAxisIndex ) = 7200.0E3;
    asterixInitialStateInKeplerianElements( eccentricityIndex ) = 0.05;
    asterixInitialStateInKeplerianElements( inclinationIndex ) = unit_conversions::convertDegreesToRadians( 85.3 );
    asterixInitialStateInKeplerianElements( argumentOfPeriapsisIndex )
            = unit_conversions::convertDegreesToRadians( 235.7 );
    asterixInitialStateInKeplerianElements( longitudeOfAscendingNodeIndex )
            = unit_conversions::convertDegreesToRadians( 23.4 );
    asterixInitialStateInKeplerianElements( trueAnomalyIndex ) = unit_conversions::convertDegreesToRadians( 139.87 );

    double earthGravitationalParameter = bodyMap.at( "Earth" )->getGravityFieldModel( )->getGravitationalParameter( );

    // Set (perturbed) initial state.
    Eigen::Matrix< StateScalarType, 6, 1 > systemInitialState = convertKeplerianToCartesianElements(
                asterixInitialStateInKeplerianElements, earthGravitationalParameter );

    // Create propagator settings
    boost::shared_ptr< TranslationalStatePropagatorSettings< StateScalarType > > propagatorSettings =
            boost::make_shared< TranslationalStatePropagatorSettings< StateScalarType > >
            ( centralBodies, accelerationModelMap, bodiesToIntegrate, systemInitialState,
              TimeType( finalEphemerisTime ), cowell );

    // Create integrator settings
    boost::shared_ptr< IntegratorSettings< TimeType > > integratorSettings =
            boost::make_shared< RungeKuttaVariableStepSizeSettings< TimeType > >
            ( rungeKuttaVariableStepSize, TimeType( initialEphemerisTime ), 40.0,
              RungeKuttaCoefficients::CoefficientSets::rungeKuttaFehlberg78,
              40.0, 40.0, 1.0, 1.0 );

    // Define parameters.
    std::vector< LinkEnds > stationReceiverLinkEnds;
    std::vector< LinkEnds > stationTransmitterLinkEnds;

    for( unsigned int i = 0; i < groundStationNames.size( ); i++ )
    {
        LinkEnds linkEnds;
        linkEnds[ transmitter ] = std::make_pair( "Earth", groundStationNames.at( i ) );
        linkEnds[ receiver ] = std::make_pair( "Vehicle", "" );
        stationTransmitterLinkEnds.push_back( linkEnds );

        linkEnds.clear( );
        linkEnds[ receiver ] = std::make_pair( "Earth", groundStationNames.at( i ) );
        linkEnds[ transmitter ] = std::make_pair( "Vehicle", "" );
        stationReceiverLinkEnds.push_back( linkEnds );
    }

    std::map< ObservableType, std::vector< LinkEnds > > linkEndsPerObservable;
    linkEndsPerObservable[ one_way_range ].push_back( stationReceiverLinkEnds[ 0 ] );
    linkEndsPerObservable[ one_way_range ].push_back( stationTransmitterLinkEnds[ 0 ] );
    linkEndsPerObservable[ one_way_range ].push_back( stationReceiverLinkEnds[ 1 ] );

    linkEndsPerObservable[ one_way_doppler ].push_back( stationReceiverLinkEnds[ 1 ] );
    linkEndsPerObservable[ one_way_doppler ].push_back( stationTransmitterLinkEnds[ 2 ] );

    linkEndsPerObservable[ angular_position ].push_back( stationReceiverLinkEnds[ 2 ] );
    linkEndsPerObservable[ angular_position ].push_back( stationTransmitterLinkEnds[ 1 ] );

    std::cout << "Link ends " << getLinkEndsString( linkEndsPerObservable[ one_way_doppler ].at( 0 ) ) << std::endl;

    std::vector< boost::shared_ptr< EstimatableParameterSettings > > parameterNames;
    parameterNames.push_back(
                boost::make_shared< InitialTranslationalStateEstimatableParameterSettings< StateScalarType > >(
                    "Vehicle", systemInitialState, "Earth" ) );

    if( useFullParameterSet )
    {
        parameterNames.push_back( boost::make_shared< EstimatableParameterSettings >( "Vehicle", radiation_pressure_coefficient ) );
        parameterNames.push_back( boost::make_shared< EstimatableParameterSettings >( "Vehicle", constant_drag_coefficient ) );
        parameterNames.push_back( boost::make_shared< ConstantObservationBiasEstimatableParameterSettings >(
                                      linkEndsPerObservable.at( one_way_range ).at( 0 ), one_way_range, true ) );
        parameterNames.push_back( boost::make_shared< ConstantObservationBiasEstimatableParameterSettings >(
                                      linkEndsPerObservable.at( one_way_range ).at( 0 ), one_way_range, false ) );
        parameterNames.push_back( boost::make_shared< ConstantObservationBiasEstimatableParameterSettings >(
                                      linkEndsPerObservable.at( one_way_range ).at( 1 ), one_way_range, false ) );

        parameterNames.push_back( boost::make_shared< SphericalHarmonicEstimatableParameterSettings >(
                                      2, 0, 2, 2, "Earth", spherical_harmonics_cosine_coefficient_block ) );
        parameterNames.push_back( boost::make_shared< SphericalHarmonicEstimatableParameterSettings >(
                                      2, 1, 2, 2, "Earth", spherical_harmonics_sine_coefficient_block ) );
        parameterNames.push_back(  boost::make_shared< EstimatableParameterSettings >
                                   ( "Earth", rotation_pole_position ) );
        parameterNames.push_back(  boost::make_shared< EstimatableParameterSettings >
                                   ( "Earth", ground_station_position, "Station1" ) );
    }

    // Create parameters
    boost::shared_ptr< estimatable_parameters::EstimatableParameterSet< StateScalarType > > parametersToEstimate =
            createParametersToEstimate( parameterNames, bodyMap );

    printEstimatableParameterEntries( parametersToEstimate );

    observation_models::ObservationSettingsMap observationSettingsMap;
    for( std::map< ObservableType, std::vector< LinkEnds > >::iterator linkEndIterator = linkEndsPerObservable.begin( );
         linkEndIterator != linkEndsPerObservable.end( ); linkEndIterator++ )
    {
        ObservableType currentObservable = linkEndIterator->first;


        std::vector< LinkEnds > currentLinkEndsList = linkEndIterator->second;
        for( unsigned int i = 0; i < currentLinkEndsList.size( ); i++ )
        {
            boost::shared_ptr< ObservationBiasSettings > biasSettings;
            if( ( currentObservable == one_way_range ) && ( i == 0 ) )
            {
                std::vector< boost::shared_ptr< ObservationBiasSettings > > biasSettingsList;

                biasSettingsList.push_back( boost::make_shared< ConstantObservationBiasSettings >(
                                                Eigen::Vector1d::Zero( ) ) );
                biasSettingsList.push_back( boost::make_shared< ConstantRelativeObservationBiasSettings >(
                                                Eigen::Vector1d::Zero( ) ) );
                biasSettings = boost::make_shared< MultipleObservationBiasSettings >(
                            biasSettingsList );
            }
            else if( ( currentObservable == one_way_range ) && ( i == 1 ) )
            {
                biasSettings = boost::make_shared< ConstantRelativeObservationBiasSettings >(
                            Eigen::Vector1d::Zero( ) );
            }

            observationSettingsMap.insert(
                        std::make_pair( currentLinkEndsList.at( i ),
                                        boost::make_shared< ObservationSettings >(
                                            currentObservable, boost::shared_ptr< LightTimeCorrectionSettings >( ),
                                            biasSettings ) ) );
        }
    }

    // Create orbit determination object.
    OrbitDeterminationManager< StateScalarType, TimeType > orbitDeterminationManager =
            OrbitDeterminationManager< StateScalarType, TimeType >(
                bodyMap, parametersToEstimate, observationSettingsMap,
                integratorSettings, propagatorSettings );

    std::vector< TimeType > baseTimeList;
    double observationTimeStart = initialEphemerisTime + 1000.0;
    double  observationInterval = 20.0;
    for( int i = 0; i < numberOfDaysOfData; i++ )
    {
        for( unsigned int j = 0; j < 500; j++ )
        {
            baseTimeList.push_back( observationTimeStart + static_cast< double >( i ) * 86400.0 +
                                    static_cast< double >( j ) * observationInterval );
        }
    }
    std::map< ObservableType, std::map< LinkEnds, std::pair< std::vector< TimeType >, LinkEndType > > > measurementSimulationInput;
    for( std::map< ObservableType, std::vector< LinkEnds > >::iterator linkEndIterator = linkEndsPerObservable.begin( );
         linkEndIterator != linkEndsPerObservable.end( ); linkEndIterator++ )
    {
        ObservableType currentObservable = linkEndIterator->first;
        std::vector< LinkEnds > currentLinkEndsList = linkEndIterator->second;
        for( unsigned int i = 0; i < currentLinkEndsList.size( ); i++ )
        {
            measurementSimulationInput[ currentObservable ][ currentLinkEndsList.at( i ) ] =
                    std::make_pair( baseTimeList, receiver );
        }
    }


    typedef Eigen::Matrix< StateScalarType, Eigen::Dynamic, 1 > ObservationVectorType;
    typedef std::map< LinkEnds, std::pair< ObservationVectorType, std::pair< std::vector< TimeType >, LinkEndType > > > SingleObservablePodInputType;
    typedef std::map< ObservableType, SingleObservablePodInputType > PodInputDataType;

    // Simulate observations
    PodInputDataType observationsAndTimes = simulateObservations< StateScalarType, TimeType >(
                measurementSimulationInput, orbitDeterminationManager.getObservationSimulators( ) );

    // Perturb parameter estimate
    Eigen::Matrix< StateScalarType, Eigen::Dynamic, 1 > initialParameterEstimate =
            parametersToEstimate->template getFullParameterValues< StateScalarType >( );
    Eigen::Matrix< StateScalarType, Eigen::Dynamic, 1 > truthParameters = initialParameterEstimate;
    Eigen::Matrix< StateScalarType, Eigen::Dynamic, 1 > parameterPerturbation =
            Eigen::Matrix< StateScalarType, Eigen::Dynamic, 1 >::Zero( truthParameters.rows( ) );

    if( numberOfIterations > 0 )
    {
        parameterPerturbation.segment( 0, 3 ) = Eigen::Vector3d::Constant( 1.0 );
        parameterPerturbation.segment( 3, 3 ) = Eigen::Vector3d::Constant( 1.E-3 );

        if( useFullParameterSet )
        {
            parameterPerturbation( 6 ) = 0.05;
            parameterPerturbation( 7 ) = 0.05;
        }
        initialParameterEstimate += parameterPerturbation;
    }


    // Define estimation input
    boost::shared_ptr< PodInput< StateScalarType, TimeType  > > podInput =
            boost::make_shared< PodInput< StateScalarType, TimeType > >(
                observationsAndTimes, initialParameterEstimate.rows( ),
                Eigen::MatrixXd::Zero( truthParameters.rows( ), truthParameters.rows( ) ),
                initialParameterEstimate - truthParameters );

    std::map< observation_models::ObservableType, double > weightPerObservable;
    weightPerObservable[ one_way_range ] = 1.0 / ( 1.0 * 1.0 );
    weightPerObservable[ angular_position ] = 1.0 / ( 1.0E-5 * 1.0E-5 );
    weightPerObservable[ one_way_doppler ] = 1.0 / ( 1.0E-11 * 1.0E-11 );

    podInput->setConstantPerObservableWeightsMatrix( weightPerObservable );
    podInput->defineEstimationSettings( true, true, true, true, false );

    // Perform estimation
    boost::shared_ptr< PodOutput< StateScalarType > > podOutput = orbitDeterminationManager.estimateParameters(
                podInput, boost::make_shared< EstimationConvergenceChecker >( numberOfIterations ) );

    Eigen::VectorXd estimationError = podOutput->parameterEstimate_ - truthParameters;
    std::cout << ( estimationError ).transpose( ) << std::endl;

    podData = std::make_pair( podOutput, podInput );

    return estimationError;
}

}

}
#endif // ORBITDETERMINATIONTESTCASES_H
