// -----------------------------------------------------------------------------

// Copyright 2011-2013 Renato Tegon Forti
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying 
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// server.cpp : Defines the entry point for the console application.
//

#define BOOST_ALL_DYN_LINK
#define BOOST_LIB_DIAGNOSTIC

#include <boost/asio.hpp>

#include "stdafx.h"
#include <fstream>

#include <boost\application.hpp>
#include <boost\program_options.hpp>

// Plugin API
#include "..\uuid_plugin\plugin_api.hpp"

using namespace boost::application;
namespace po = boost::program_options;

class my_server
{
   // plugin entry point
   typedef my_plugin_api* (*pluginapi_create) (void);
   typedef void (*pluginapi_delete) (my_plugin_api* myplugin);

public:

   int setup(application_ctrl& ctrl)
   {
      if(ctrl.run_type() == boost::application::application_common)
         return 0; // continue

      // -----------------------------------------------------------------------------
      // This method provide a installation system to application using 
      // program_options
      //
      // [installation]
      // server.exe -i
      // /or/
      // server.exe -i --name "My Service"
      // server.exe -i --name "My Service"
      // [check]
      // server.exe -c
      // /or/
      // server.exe -c --name "My Service" 
      // [unstalation]
      // server.exe -u
      // /or/
      // server.exe -u --name "My Service" 
      // 
      // Note that when arg name are not priovided, the name will be the name of
      // executable, in this case, service name will be: 'server'
      // -----------------------------------------------------------------------------

#if defined(BOOST_WINDOWS_API)    
      
      // get our executable path name
      boost::filesystem::path executable_path_name = ctrl.executable_path_name();

      // define our simple installation schema options
      po::options_description install("service options");
      install.add_options()
         ("help", "produce a help message")
         (",i", "install service")
         (",u", "unistall service")
         (",c", "check service")
         ("name", po::value<std::string>()->default_value(ctrl.executable_name().stem().string()), "service name")
         ("display", po::value<std::string>()->default_value(""), "service display name (optional, installation only)")
         ("description", po::value<std::string>()->default_value(""), "service description (optional, installation only)")
         ;

      po::variables_map vm;
      po::store(po::parse_command_line(ctrl.argc(), ctrl.argv(), install), vm);
      boost::system::error_code ec;

      if (vm.count("help")) 
      {
         std::cout << install << std::cout;
         return 1;
      }

      if (vm.count("-i")) 
      {
         install_windows_service(
            setup_arg(vm["name"].as<std::string>()), 
            setup_arg(vm["display"].as<std::string>()), 
            setup_arg(vm["description"].as<std::string>()), 
            setup_arg(executable_path_name)).install(ec);

         std::cout << ec.message() << std::endl;

         return 1;
      }

      if (vm.count("-u")) 
      {
         uninstall_windows_service(
            setup_arg(vm["name"].as<std::string>()), 
            setup_arg(executable_path_name)).uninstall(ec);
			   
         std::cout << ec.message() << std::endl;

         return 1;
      }

      if (vm.count("-c")) 
      {
         if(check_windows_service(setup_arg(vm["name"].as<std::string>())).exist(ec))
            std::cout 
               << "Windows service '" 
               << vm["name"].as<std::string>() 
               << "' is aready installed!" 
               << std::endl;
         else
            std::cout 
               << "Windows service '" 
               << vm["name"].as<std::string>() 
               << "' is not installed!" 
               << std::endl;

         std::cout << ec.message() << std::endl;
         return 1;
      }
#endif

      // return 1 to exit, 0 to continue
      return 0;
   }

   int operator()(const std::vector< application_ctrl::string_type >& args, 
      application_ctrl& ctrl)
   {
      // launch a work thread
      boost::thread thread(boost::bind(&my_server::work_thread, this, &ctrl));

      plugin_path_ = ctrl.executable_path().wstring() + L"\\uuid_plugin" + shared_library::suffix();
      plugin_.load(library(plugin_path_));

      ctrl.wait_for_termination_request(); 
      thread.join(); // the last connection need be served to app exit, comment this to exit quick

      return 0;
   }

   int pause()
   {
      plugin_.unload();
      return 1;
   }
   
   int resume()
   {
      plugin_.load(library(plugin_path_));
      return 1;
   }

protected:

   void work_thread(boost::application::application_ctrl* ctrl)
   {
      // application logic
      using boost::asio::ip::tcp;

      try
      {
         boost::asio::io_service io_service;

         tcp::acceptor acceptor(io_service, tcp::endpoint(tcp::v4(), 9512));

         for (;;)
         {
            if(ctrl->state() == boost::application::application_stoped)
            {
               return;
            }

            boost::system::error_code error;

            tcp::socket socket(io_service);
            acceptor.accept(socket);

            // our data is limited to 1024 bytes
            char data[1024];
               
            size_t length = socket.read_some(boost::asio::buffer(data), error);

            if(error == boost::asio::error::eof)
               break; // Connection closed cleanly by peer.
            else if (error)
               throw boost::system::system_error(error); // Some other error.
         
            // response (echo)
            std::string message(data, length);
       
            // detect pause state
            if(ctrl->state() == boost::application::application_paused)
            {
               // if app is on pause state we will aswer a pause message.
               message = "application uuid engine is paused, try again later!";
            }
            else
            {
               if(plugin_.is_loaded())
               {
                  my_plugin_api* plugin = NULL;

                  if(plugin_.search_symbol(symbol(L"create_my_plugin")))
                  {
                     plugin = ((pluginapi_create)plugin_(symbol(L"create_my_plugin")))();
                  }

                  if(plugin != NULL)
                  {
                     message = plugin->transform_string(message);

                     ((pluginapi_delete)plugin_(symbol(L"delete_my_plugin")))(plugin);
                  }
               }
               else
               {
                  message = "some problem with plugin load, try again later!";
               }
            }
               
            boost::asio::write(socket, boost::asio::buffer(message), 
                  boost::asio::transfer_all(), error);
         }
      }
      catch (std::exception& e)
      {
         std::cerr << e.what() << std::endl;
      }
   }

private:

   shared_library plugin_;
   boost::filesystem::path plugin_path_;

}; // my_server class

int _tmain(int argc, _TCHAR* argv[])
{
   try 
   {
      bool as_serice = true;

      {
         po::variables_map vm;
         po::options_description desc;

         desc.add_options()
            (",h", "Shows help.")
            (",f", "Run as common applicatio")
            ("help", "produce a help message")
            (",i", "install service")
            (",u", "unistall service")
            (",c", "check service")
            ("name", po::value<std::string>()->default_value(""), "service name")
            ("display", po::value<std::string>()->default_value(""), "service display name (optional, installation only)")
            ("description", po::value<std::string>()->default_value(""), "service description (optional, installation only)")
            ;

         po::store(po::parse_command_line(argc, argv, desc), vm);

         if (vm.count("-h")) 
         {
            // show help
            std::cout << desc << std::endl;
            return 0;
         }

         if (vm.count("-f")) 
            as_serice = false;
      }

      if (!as_serice)
      {
         // instantiate server application as common_app
         return common_app<my_server>( args(argc, argv) )();
      }
      else
      {
         return application<
            application_type<server_application>, 
            accept_pause_and_resume<yes>,       
            my_application< my_server > >( args(argc, argv))();
      }
   }
   catch(boost::system::system_error& se)
   {
      std::cerr << se.what() << std::endl;
      return 1;
   }
   catch(std::exception &e)
   {
      std::cerr << e.what() << std::endl;
      return 1;
   }
   catch(...)
   {
      std::cerr << "Unknown error." << std::endl;
      return 1;
   }

   return 0;
}


