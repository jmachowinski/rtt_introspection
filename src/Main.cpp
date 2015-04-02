#include <iostream>

#include <orocos_cpp/Spawner.hpp>
#include <rtt/transports/corba/TaskContextServer.hpp>
#include <rtt/transports/corba/TaskContextProxy.hpp>
#include <orocos_cpp_base/OrocosHelpers.hpp>
#include <rtt/OperationCaller.hpp>
#include "IntrospectionTypes.hpp"
#include "ConnectionMatcher.hpp"
#include <orocos_cpp/CorbaNameService.hpp>
#include "IntrospectionService.hpp"
#include <boost/filesystem.hpp>

int main(int argc, char** argv)
{

    Spawner &spawner(Spawner::getInstace());
    
    RTT::corba::TaskContextServer::InitOrb(argc, argv);

    Deployment introTest("inspection_test");
//     spawner.spawnDeployment(&introTest, false);

    spawner.spawnTask("inspection_test::Task", "task1", false);
    spawner.spawnTask("inspection_test::Task2", "task2", false);
    
    OrocosHelpers::loadTypekitAndTransports("rtt-types");
    OrocosHelpers::loadTypekitAndTransports("rtt_introspection");
    
    
    spawner.waitUntilAllReady(base::Time::fromSeconds(10.0));
    
//     introTest.loadNeededTypekits();
    
    RTT::corba::TaskContextProxy *t1 = RTT::corba::TaskContextProxy::Create("task1", false);
    RTT::corba::TaskContextProxy *t2 = RTT::corba::TaskContextProxy::Create("task2", false);
    
    t1->getPort("output")->connectTo(t2->getPort("input2"), RTT::ConnPolicy::buffer(50));
    t2->getPort("output2")->connectTo(t1->getPort("input"));

    t1->configure();
    t2->configure();
    
    t1->start();
    t2->start();
    
    delete t1;
    delete t2;
    
    std::cout << "Connect done" << std::endl;

    CorbaNameService ns;
    
    ns.connect();
    
    auto tasks = ns.getRegisteredTasks();
    

    
    std::vector<std::string> searchedFields;
    std::vector<std::string> pkgConfResults;
    searchedFields.push_back("prefix");
    
    OrocosHelpers::parsePkgConfig("rtt_introspection.pc", searchedFields, pkgConfResults);
    
    std::string servicePath = pkgConfResults[0] + "/lib/orocos/plugins/librtt_introspection.so";
    
    if(!boost::filesystem::exists(servicePath))
    {
        throw std::runtime_error("Error: Service plugin not found on disk");
    }
    
    std::cout << "plugin dir is " << servicePath << std::endl;
    
    RTT::introspection::ConnectionMatcher matcher;
    std::string str;
    std::cout << "Press Enter to capture network state" << std::endl;
    std::getline(std::cin, str);
    
    for(const std::string &taskName: tasks)
    {        
        RTT::corba::TaskContextProxy *task = RTT::corba::TaskContextProxy::Create(taskName);
        
        RTT::OperationInterfacePart *op = nullptr;
        
        while(!(op = task->getOperation(RTT::introspection::IntrospectionService::OperationName)))
        {
            std::cout << "No operation " << RTT::introspection::IntrospectionService::OperationName << " detected on task " << taskName << " loading introspection service" << std::endl;
            RTT::OperationCaller< bool (const std::string &)> loadPlugin(task->getOperation("loadPlugin"));
            RTT::OperationCaller< bool (const std::string &)> loadService(task->getOperation("loadService"));

            
            //FIXME find way better way to do this
            loadPlugin(servicePath);
            
            loadService(RTT::introspection::IntrospectionService::ServiceName);
            
            usleep(10000);
            
            delete task;
            task = RTT::corba::TaskContextProxy::Create(taskName);
        }

        RTT::OperationCaller<RTT::introspection::TaskData ()> fc(op);
        matcher.addTaskData(fc());

        delete task;
    }    

    matcher.createGraph();
    
    matcher.printGraph();
    
    matcher.writeGraphToDotFile("test.dot");
    
    spawner.killAll();
    
    return 0;
}
