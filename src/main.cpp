#include <boost/stacktrace.hpp>
#include <string_view>
#include <filesystem>
#include <cassert>
#include <csignal>

#include "generic/math/MathUtility.hpp"
#include "generic/tools/FileSystem.hpp"
#include "generic/tools/Format.hpp"
#include "glog/logging.h"
#include "ceres/ceres.h"
#include "EDataMgr.h"

using namespace ecad;
void SignalHandler(int signum)
{
    ::signal(signum, SIG_DFL);
    std::cout << boost::stacktrace::stacktrace();
    ::raise(SIGABRT);
}

Ptr<ILayoutView> SetupDesign()
{
    auto & eDataMgr = EDataMgr::Instance();

    //database
    auto database = eDataMgr.CreateDatabase("RobGrant");

    auto matAl = database->CreateMaterialDef("Al");
    matAl->SetProperty(EMaterialPropId::ThermalConductivity, eDataMgr.CreateSimpleMaterialProp(238));
    matAl->SetProperty(EMaterialPropId::SpecificHeat, eDataMgr.CreateSimpleMaterialProp(880));
    matAl->SetProperty(EMaterialPropId::MassDensity, eDataMgr.CreateSimpleMaterialProp(2700));
    matAl->SetProperty(EMaterialPropId::Resistivity, eDataMgr.CreateSimpleMaterialProp(2.82e-8));

    auto matCu = database->CreateMaterialDef("Cu");
    matCu->SetProperty(EMaterialPropId::ThermalConductivity, eDataMgr.CreateSimpleMaterialProp(398));
    matCu->SetProperty(EMaterialPropId::SpecificHeat, eDataMgr.CreateSimpleMaterialProp(380));
    matCu->SetProperty(EMaterialPropId::MassDensity, eDataMgr.CreateSimpleMaterialProp(8850));

    auto matAir = database->CreateMaterialDef("Air");
    matAir->SetMaterialType(EMaterialType::Fluid);
    matAir->SetProperty(EMaterialPropId::ThermalConductivity, eDataMgr.CreateSimpleMaterialProp(0.026));
    matAir->SetProperty(EMaterialPropId::SpecificHeat, eDataMgr.CreateSimpleMaterialProp(1.003));
    matAir->SetProperty(EMaterialPropId::MassDensity, eDataMgr.CreateSimpleMaterialProp(1.225));

    auto matSiC = database->CreateMaterialDef("SiC");  
    matSiC->SetProperty(EMaterialPropId::ThermalConductivity, eDataMgr.CreateSimpleMaterialProp(370));
    matSiC->SetProperty(EMaterialPropId::SpecificHeat, eDataMgr.CreateSimpleMaterialProp(750));
    matSiC->SetProperty(EMaterialPropId::MassDensity, eDataMgr.CreateSimpleMaterialProp(3210));

    auto matSi3N4 = database->CreateMaterialDef("Si3N4");
    matSi3N4->SetProperty(EMaterialPropId::ThermalConductivity, eDataMgr.CreateSimpleMaterialProp(70));
    matSi3N4->SetProperty(EMaterialPropId::SpecificHeat, eDataMgr.CreateSimpleMaterialProp(691));
    matSi3N4->SetProperty(EMaterialPropId::MassDensity, eDataMgr.CreateSimpleMaterialProp(2400));

    auto matSolder = database->CreateMaterialDef("Sn-3.5Ag");
    matSolder->SetProperty(EMaterialPropId::ThermalConductivity, eDataMgr.CreateSimpleMaterialProp(33));
    matSolder->SetProperty(EMaterialPropId::SpecificHeat, eDataMgr.CreateSimpleMaterialProp(200));
    matSolder->SetProperty(EMaterialPropId::MassDensity, eDataMgr.CreateSimpleMaterialProp(7360));
    matSolder->SetProperty(EMaterialPropId::Resistivity, eDataMgr.CreateSimpleMaterialProp(11.4e-8));

    //coord units
    ECoordUnits coordUnits(ECoordUnits::Unit::Micrometer);
    database->SetCoordUnits(coordUnits);
    
    //top cell
    auto topCell = eDataMgr.CreateCircuitCell(database, "TopCell");
    auto topLayout = topCell->GetLayoutView();
    auto topBouds = std::make_unique<EPolygon>(eDataMgr.CreatePolygon(coordUnits, {{-5000, -5000}, {86000, -5000}, {86000, 31000}, {-5000, 31000}}));
    topLayout->SetBoundary(std::move(topBouds));

    eDataMgr.CreateNet(topLayout, "Gate");
    eDataMgr.CreateNet(topLayout, "Drain");
    eDataMgr.CreateNet(topLayout, "Source");

    //substrate
    [[maybe_unused]] auto iLyrTopCu = topLayout->AppendLayer(eDataMgr.CreateStackupLayer("TopCu", ELayerType::ConductingLayer, 0, 400, matCu->GetName(), matAir->GetName()));
    [[maybe_unused]] auto iLyrSubstrate = topLayout->AppendLayer(eDataMgr.CreateStackupLayer("Substrate", ELayerType::DielectricLayer, -400, 635, matSi3N4->GetName(), matSi3N4->GetName()));
    [[maybe_unused]] auto iLyrCuPlate = topLayout->AppendLayer(eDataMgr.CreateStackupLayer("CuPlate", ELayerType::ConductingLayer, -1035, 300, matCu->GetName(), matCu->GetName()));
    assert(iLyrTopCu != ELayerId::noLayer);
    assert(iLyrSubstrate != ELayerId::noLayer);
    assert(iLyrCuPlate != ELayerId::noLayer);

    //sic die
    auto sicCell = eDataMgr.CreateCircuitCell(database, "SicDie");
    assert(sicCell);
    auto sicLayout = sicCell->GetLayoutView();
    assert(sicLayout);

    //boundary
    auto sicBonds = std::make_unique<EPolygon>(eDataMgr.CreatePolygon(coordUnits, {{0, 0}, {23000, 0}, {23000, 26000}, {0, 26000}}));
    sicLayout->SetBoundary(std::move(sicBonds));

    auto iLyrWire = sicLayout->AppendLayer(eDataMgr.CreateStackupLayer("Wire", ELayerType::ConductingLayer, 0, 400, matCu->GetName(), matAir->GetName()));
    assert(iLyrWire != ELayerId::noLayer);

    //component
    auto compDef = eDataMgr.CreateComponentDef(database, "CPMF-1200-S080B Z-FET");
    assert(compDef);
    compDef->SetSolderBallBumpHeight(100);
    compDef->SetSolderFillingMaterial(matSolder->GetName());
    compDef->SetBondingBox(eDataMgr.CreateBox(coordUnits, FPoint2D(-2000, -2000), FPoint2D(2000, 2000)));
    compDef->SetMaterial(matSiC->GetName());
    compDef->SetHeight(365);

    eDataMgr.CreateComponentDefPin(compDef, "Gate1", {-1000,  1000}, EPinIOType::Receiver);
    eDataMgr.CreateComponentDefPin(compDef, "Gate2", {-1000, -1000}, EPinIOType::Receiver);
    eDataMgr.CreateComponentDefPin(compDef, "Source1", {1000,  1000}, EPinIOType::Receiver);
    eDataMgr.CreateComponentDefPin(compDef, "Source2", {1000, -1000}, EPinIOType::Receiver);
    
    bool flipped{false};
    EFloat comp1x = 2000, comp1y = 12650;
    EFloat comp2x = 17750, comp2y = 12650; 
    [[maybe_unused]] auto comp1 = eDataMgr.CreateComponent(sicLayout, "M1", compDef, iLyrWire, eDataMgr.CreateTransform2D(coordUnits, 1, 0, {comp1x, comp1y}), flipped);
    [[maybe_unused]] auto comp2 = eDataMgr.CreateComponent(sicLayout, "M2", compDef, iLyrWire, eDataMgr.CreateTransform2D(coordUnits, 1, 0, {comp2x, comp2y}, EMirror2D::Y), flipped);
    assert(comp1);
    assert(comp2);
    comp1->SetLossPower(33.8);
    comp2->SetLossPower(31.9);
 
    //net
    auto gateNet = eDataMgr.CreateNet(sicLayout, "Gate");
    auto drainNet = eDataMgr.CreateNet(sicLayout, "Drain");
    auto sourceNet = eDataMgr.CreateNet(sicLayout, "Source");

    //wire
    EFloat bwRadius = 250;//um
    std::vector<FPoint2D> ps1 {{0, 0}, {14200, 0}, {14200, 3500}, {5750, 3500}, {5750, 9150}, {0, 9150}};
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, sourceNet->GetNetId(), eDataMgr.CreateShapePolygon(coordUnits, std::move(ps1)));

    std::vector<FPoint2D> ps2 {{0, 10650}, {7300, 10650}, {7300, 5000}, {14300, 5000}, {14300, 19000}, {1450, 19000}, {1450, 26000}, {0, 26000}};
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, drainNet->GetNetId(), eDataMgr.CreateShapePolygon(coordUnits, std::move(ps2)));

    std::vector<FPoint2D> ps3 {{15750, 0}, {23000, 0}, {23000, 18850}, {18000, 18850}, {18000, 26000}, {14500, 26000}, {14500, 20500}, {15750, 20500}};
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, drainNet->GetNetId(), eDataMgr.CreateShapePolygon(coordUnits, std::move(ps3)));

    auto rec1 = eDataMgr.CreateShapeRectangle(coordUnits, FPoint2D(2500, 20500), FPoint2D(4000, 26000));
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, gateNet->GetNetId(), std::move(rec1));

    auto rec2 = eDataMgr.CreateShapeRectangle(coordUnits, FPoint2D(5000, 20500), FPoint2D(6500, 26000));
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, gateNet->GetNetId(), std::move(rec2));

    auto rec3 = eDataMgr.CreateShapeRectangle(coordUnits, FPoint2D(7500, 20500), FPoint2D(13500, 23000));
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, ENetId::noNet, std::move(rec3));

    auto rec4 = eDataMgr.CreateShapeRectangle(coordUnits, FPoint2D(7500, 24000), FPoint2D(10000, 26000));
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, ENetId::noNet, std::move(rec4));

    auto rec5 = eDataMgr.CreateShapeRectangle(coordUnits, FPoint2D(11000, 24000), FPoint2D(13500, 26000));
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, ENetId::noNet, std::move(rec5));

    auto rec6 = eDataMgr.CreateShapeRectangle(coordUnits, FPoint2D(19000, 20500), FPoint2D(20500, 26000));
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, gateNet->GetNetId(), std::move(rec6));

    auto rec7 = eDataMgr.CreateShapeRectangle(coordUnits, FPoint2D(21500, 20500), FPoint2D(23000, 26000));
    eDataMgr.CreateGeometry2D(sicLayout, iLyrWire, gateNet->GetNetId(), std::move(rec7));

    //bondwire
    auto sourceBW1 = eDataMgr.CreateBondwire(sicLayout, "SourceBW1", sourceNet->GetNetId(), bwRadius);
    sourceBW1->SetBondwireType(EBondwireType::JEDEC4);
    sourceBW1->SetStartComponent(comp1, "Source1");
    sourceBW1->SetEndLayer(iLyrWire, coordUnits.toCoord(FPoint2D{2500, 8700}), false);
    sourceBW1->SetCurrent(20);

    auto sourceBW2 = eDataMgr.CreateBondwire(sicLayout, "SourceBW2", sourceNet->GetNetId(), bwRadius);
    sourceBW2->SetBondwireType(EBondwireType::JEDEC4);
    sourceBW2->SetStartComponent(comp1, "Source2");
    sourceBW2->SetEndLayer(iLyrWire, coordUnits.toCoord(FPoint2D{3500, 8700}), false);
    sourceBW2->SetCurrent(20);

    auto sourceBW3 = eDataMgr.CreateBondwire(sicLayout, "SourceBW3", sourceNet->GetNetId(), bwRadius);
    sourceBW3->SetStartComponent(comp1, "Source1");
    sourceBW3->SetEndComponent(comp2, "Source1");
    sourceBW3->SetCurrent(10);

    auto sourceBW4 = eDataMgr.CreateBondwire(sicLayout, "SourceBW4", sourceNet->GetNetId(), bwRadius);
    sourceBW4->SetStartComponent(comp1, "Source2");
    sourceBW4->SetEndComponent(comp2, "Source2");
    sourceBW4->SetCurrent(10);

    EFloat drainBWStartX = 13500, drainBWEndX = 16500;
    auto drainBW1 = eDataMgr.CreateBondwire(sicLayout, "DrainBW1", drainNet->GetNetId(), bwRadius);
    drainBW1->SetStartLayer(iLyrWire, coordUnits.toCoord(FPoint2D{drainBWStartX,  8200}), false);
    drainBW1->SetEndLayer(iLyrWire, coordUnits.toCoord(FPoint2D{drainBWEndX, 3500}), false);

    auto drainBW2 = eDataMgr.CreateBondwire(sicLayout, "DrainBW2", drainNet->GetNetId(), bwRadius);
    drainBW2->SetStartLayer(iLyrWire, coordUnits.toCoord(FPoint2D{drainBWStartX,  6200}), false);
    drainBW2->SetEndLayer(iLyrWire, coordUnits.toCoord(FPoint2D{drainBWEndX, 1500}), false);

    EFloat gateBWEndY{21000};
    auto gateBW1 = eDataMgr.CreateBondwire(sicLayout, "GateBW1", gateNet->GetNetId(), bwRadius);
    gateBW1->SetBondwireType(EBondwireType::JEDEC4); 
    gateBW1->SetStartComponent(comp1, "Gate1");
    gateBW1->SetEndLayer(iLyrWire, coordUnits.toCoord(FPoint2D{3250, gateBWEndY}), false);

    auto gateBW2 = eDataMgr.CreateBondwire(sicLayout, "GateBW2", gateNet->GetNetId(), bwRadius);
    gateBW2->SetBondwireType(EBondwireType::JEDEC4);
    gateBW2->SetStartComponent(comp1, "Gate2");
    gateBW2->SetEndLayer(iLyrWire, coordUnits.toCoord(FPoint2D{5750, gateBWEndY}), false);

    auto gateBW3 = eDataMgr.CreateBondwire(sicLayout, "GateBW3", gateNet->GetNetId(), bwRadius);
    gateBW3->SetBondwireType(EBondwireType::JEDEC4);
    gateBW3->SetStartComponent(comp2, "Gate1");
    gateBW3->SetEndLayer(iLyrWire, coordUnits.toCoord(FPoint2D{19750, gateBWEndY}), false);

    auto gateBW4 = eDataMgr.CreateBondwire(sicLayout, "GateBW4", gateNet->GetNetId(), bwRadius);
    gateBW4->SetBondwireType(EBondwireType::JEDEC4);
    gateBW4->SetStartComponent(comp2, "Gate2");
    gateBW4->SetEndLayer(iLyrWire, coordUnits.toCoord(FPoint2D{22250, gateBWEndY}), false);
    
    auto bondwireSolderDef = eDataMgr.CreatePadstackDef(database, "Bondwire Solder Joints");
    auto bondwireSolderDefData = eDataMgr.CreatePadstackDefData();
    bondwireSolderDefData->SetTopSolderBumpMaterial(matSolder->GetName());
    bondwireSolderDefData->SetBotSolderBallMaterial(matSolder->GetName());
    
    auto bumpR = bwRadius * 1.2 * 1e3;
    auto topBump = eDataMgr.CreateShapeCircle({0, 0}, bumpR);
    bondwireSolderDefData->SetTopSolderBumpParameters(std::move(topBump), 100);
    
    auto botBall = eDataMgr.CreateShapeCircle({0, 0}, bumpR);
    bondwireSolderDefData->SetBotSolderBallParameters(std::move(botBall), 100);

    bondwireSolderDef->SetPadstackDefData(std::move(bondwireSolderDefData));

    auto primIter = sicLayout->GetPrimitiveIter();
    while (auto * prim = primIter->Next()) {
        if (auto * bw = prim->GetBondwireFromPrimitive(); bw) {
            bw->SetSolderJoints(bondwireSolderDef);
            bw->SetMaterial(matAl->GetName());
            bw->SetHeight(500);
        }
    }

    //layer map
    auto layerMap = eDataMgr.CreateLayerMap(database, "Layermap");
    layerMap->SetMapping(iLyrWire, iLyrTopCu);

    //instance
    auto inst1 = eDataMgr.CreateCellInst(topLayout, "Inst1", sicLayout, eDataMgr.CreateTransform2D(coordUnits, 1, 0, {0, 0}));
    inst1->SetLayerMap(layerMap);

    auto inst2 = eDataMgr.CreateCellInst(topLayout, "Inst2", sicLayout, eDataMgr.CreateTransform2D(coordUnits, 1, 0, {29000, 0}));
    inst2->SetLayerMap(layerMap);

    auto inst3 = eDataMgr.CreateCellInst(topLayout, "Inst3", sicLayout, eDataMgr.CreateTransform2D(coordUnits, 1, 0, {58000, 0}));
    inst3->SetLayerMap(layerMap);

    //flatten
    database->Flatten(topCell);
    
    return topCell->GetFlattenedLayoutView();
}

struct CostFunctor
{
    explicit CostFunctor(CPtr<ILayoutView> layout) : m_layout(layout)
    {
        m_compIdxMap = std::unordered_map<size_t, std::string>{
            {0, "Inst1/M1"}, {1, "Inst2/M1"}, {2, "Inst3/M1"}, {3, "Inst1/M2"}, {4, "Inst2/M2"}, {5, "Inst3/M2"}
        };
    }

    bool operator() (const double* const parameters, double* residual) const
    {
        std::cout << "test paras: ";
        for (size_t i = 0; i < 12; ++i) {
            std::cout << parameters[i] << ", ";
        }
        for (size_t i = 0; i < 12; ++i) {
            if (parameters[i] < 0 || 1 < parameters[i]) return false;
        }
        auto clone = m_layout->Clone();
        const auto & coordUnits = m_layout->GetCoordUnits();
        for (size_t i = 0; i < 3; ++i) {
            auto comp = clone->FindComponentByName(m_compIdxMap.at(i));
            ECAD_TRACE("comp: %1%", comp->GetName())
            FVector2D shift(parameters[i * 2 + 0] * 10300, parameters[i * 2 + 1] * 4350);
            auto transform = EDataMgr::Instance().CreateTransform2D(coordUnits, 1.0, 0.0, shift);
            comp->AddTransform(transform);
        }

        for (size_t i = 3; i < 6; ++i) {
            auto comp = clone->FindComponentByName(m_compIdxMap.at(i));
            ECAD_TRACE("comp: %1%", comp->GetName())
            FVector2D shift(parameters[i * 2 + 0] * 3250, parameters[i * 2 + 1] * 4200);
            auto transform = EDataMgr::Instance().CreateTransform2D(coordUnits, 1.0, 0.0, shift);
            comp->AddTransform(transform);
        }

        EPrismaThermalModelExtractionSettings prismaSettings;
        prismaSettings.workDir = generic::fs::CurrentPath();
        prismaSettings.meshSettings.iteration = 1e5;
        prismaSettings.meshSettings.minAlpha = 20;
        prismaSettings.meshSettings.minLen = 1e-2;
        prismaSettings.meshSettings.maxLen = 5000;

        EThermalSimulationSetup setup;
        setup.simuType = EThermalSimuType::Static;
        setup.environmentTemperature = 25;
        setup.workDir = prismaSettings.workDir;
        residual[0] = clone->RunThermalSimulation(prismaSettings, setup); 
        std::cout << "maxT: " << residual[0] << std::endl;
        return true;
    }
private:
    CPtr<ILayoutView> m_layout;
    std::unordered_map<size_t, std::string> m_compIdxMap;
};

// int main(int argc, char * argv[])
// {
//     ::signal(SIGSEGV, &SignalHandler);
//     ::signal(SIGABRT, &SignalHandler);

//     google::InitGoogleLogging(argv[0]);
//     EDataMgr::Instance().Init();

//     auto layout = SetupDesign();

//     std::vector<double> residual(1, 0);
//     std::vector<double> parameters(12, 0.5);

//     ceres::Problem problem;
//     auto costFunc = new ceres::NumericDiffCostFunction<CostFunctor, ceres::CENTRAL, 1, 12>(new CostFunctor(layout));
//     // problem.AddResidualBlock(costFunc, new ceres::CauchyLoss(0.5), parameters.data());
//     problem.AddResidualBlock(costFunc, nullptr, parameters.data());

//     for (size_t i = 0; i < 12; ++i) {
//         problem.SetParameterLowerBound(parameters.data(), i, 0.01);
//         problem.SetParameterUpperBound(parameters.data(), i, 0.99);
//     }

//     ceres::Solver::Options options;
// 	options.num_threads = 15;
// 	options.max_num_iterations = 1e4;
// 	options.minimizer_progress_to_stdout = true;
// 	options.linear_solver_type = ceres::DENSE_QR;
// 	// options.trust_region_strategy_type = ceres::DOGLEG;
// 	options.logging_type = ceres::SILENT;
//     options.min_line_search_step_size = 1e-3;

//     ceres::Solver::Summary summary;
//     ceres::Solve(options, &problem, &summary);
//     std::cout << summary.BriefReport() << "\n";

//     std::cout << "paras: " << generic::fmt::Fmt2Str(parameters, ",");
//     return EXIT_SUCCESS;
// }

std::vector<double> RandomSolution()
{
    std::vector<double> solution(12);
    for (auto & value : solution)
        value = generic::math::Random<double>(0, 1);
    return solution;
}

std::pair<std::vector<double>, double> SimulatedAnnealing(CPtr<ILayoutView> layout, double initialTemperature, double coolingRate, size_t maxIteration)
{
    auto currentSolution = RandomSolution();
    double currentCost{0};
    CostFunctor costFunctor(layout);
    costFunctor(currentSolution.data(), &currentCost);
    auto bestCost = currentCost;
    auto bestSolution = currentSolution;
    for (size_t i = 0; i < maxIteration; ++i) {
        auto newSolution = RandomSolution();
        double newCost;
        CostFunctor costFunctor(layout);
        costFunctor(newSolution.data(), &newCost);
        
        auto deltaCost = newCost - currentCost;
        auto acceptanceProbability = exp(-deltaCost / initialTemperature);

        if (deltaCost < 0 || acceptanceProbability > generic::math::Random<double>(0, 1) / 1.0) {
            currentSolution = newSolution;
            currentCost = newCost;
        }

        if (currentCost < bestCost) {
            bestSolution = currentSolution;
            bestCost = currentCost;
        }

        initialTemperature *= coolingRate;
    }

    return {bestSolution, bestCost};
}

int main(int argc, char * argv[])
{
    ::signal(SIGSEGV, &SignalHandler);
    ::signal(SIGABRT, &SignalHandler);

    EDataMgr::Instance().Init();

    auto layout = SetupDesign();

    auto [bestSolution, bestCost] = SimulatedAnnealing(layout, 100, 0.95, 1e3);
    std::cout << "solution: " << generic::fmt::Fmt2Str(bestSolution, ",") << ", maxT: " << bestCost << std::endl;

    return EXIT_SUCCESS;
}