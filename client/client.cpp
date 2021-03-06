// -----------------------------------------------------------------------------

// Copyright 2011-2013 Renato Tegon Forti
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying 
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

// client.cpp : Defines the entry point for the console application.
//

#define BOOST_ALL_DYN_LINK
#define BOOST_LIB_DIAGNOSTIC

#include <boost/asio.hpp>

#include "stdafx.h"

#include <boost\application.hpp>
#include <boost\program_options.hpp>

using namespace boost::application;
namespace po = boost::program_options;

class my_client
{
public:

   int operator()(const std::vector< application_ctrl::string_type >& args, 
      application_ctrl& ctrl)
   {
      using boost::asio::ip::tcp;

      // define our simple installation schema options
      po::options_description cli("client options");
      cli.add_options()
         ("help", "produce a help message")
         (",s", po::value<std::string>()->default_value("test"), "string to send")
         ;

      po::variables_map vm;
      po::store(po::parse_command_line(ctrl.argc(), ctrl.argv(), cli), vm);

      if (vm.count("help")) 
      {
         std::cout << cli << std::cout;
         return 1;
      }

      std::string message_string = vm["-s"].as<std::string>();

      if(message_string.size() > 1024)
      {
         std::cerr << "Message is too long!" << std::endl; 
      }

      std::cout 
         << "We will send to server : " 
         << message_string
         << std::endl;

      boost::system::error_code error;
      boost::asio::io_service io_service;

      tcp::resolver resolver(io_service);
      tcp::resolver::query query(tcp::v4(), "localhost", "9512");
      tcp::resolver::iterator iterator = resolver.resolve(query);

      tcp::socket socket(io_service);
      boost::asio::connect(socket, iterator);

      boost::asio::write(socket, boost::asio::buffer(message_string), 
               boost::asio::transfer_all(), error);

      char reply[1024];
      size_t reply_length = socket.read_some(boost::asio::buffer(reply), error);

      std::cout << "Reply is: ";
      std::cout.write(reply, reply_length);
      std::cout << "\n";
      
      return 0;
   }

}; // my_client class

int _tmain(int argc, _TCHAR* argv[])
{
   try 
   {
      return application<
         my_application< my_client > >( args(argc, argv))();
   }
   catch(boost::system::system_error& se)
   {
      std::cerr << se.what() << std::endl;
      return 1;
   }
}
