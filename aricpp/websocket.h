/*******************************************************************************
 * ARICPP - ARI interface for C++
 * Copyright (C) 2017 Daniele Pallastrelli
 *
 * This file is part of aricpp.
 * For more information, see http://github.com/daniele77/aricpp
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************/


#ifndef ARICPP_WEBSOCKET_H_
#define ARICPP_WEBSOCKET_H_

#include <string>
#include <boost/asio.hpp>
#include <beast/core.hpp>
#include <beast/websocket.hpp>

//#define ARICPP_TRACE_WEBSOCKET

namespace aricpp
{

class WebSocket
{
public:

    using ConnectHandler = std::function< void( const boost::system::error_code& ) >;
    using ReceiveHandler = std::function< void( const std::string&, const boost::system::error_code& ) >;

    WebSocket( boost::asio::io_service& _ios, std::string _host, std::string _port ) :
        ios(_ios), host(std::move(_host)), port(std::move(_port)), resolver(ios), socket(ios), websocket(socket)
    {}

    WebSocket() = delete;
    WebSocket( const WebSocket& ) = delete;
    WebSocket( WebSocket&& ) = delete;
    WebSocket& operator = ( const WebSocket& ) = delete;

    ~WebSocket()
    {
        Close();
    }

    void Connect( const std::string& req, const ConnectHandler& h )
    {
        request = req;
        onConnection = h;
        resolver.async_resolve(
            boost::asio::ip::tcp::resolver::query{ host, port },
            [this]( const boost::system::error_code& ec, boost::asio::ip::tcp::resolver::iterator i )
            {
                if ( ec ) onConnection( ec );
                else Resolved( i );
            }
        );
    }

    void Close()
    {
        if ( socket.is_open() )
        {
            websocket.close( beast::websocket::close_code::normal );
            socket.cancel();
            socket.close();
        }
    }

    void Receive( const ReceiveHandler& h )
    {
        onReceive = h;
        Read();
    }

private:

    void Resolved( boost::asio::ip::tcp::resolver::iterator i )
    {
        boost::asio::async_connect(
            socket,
            i,
            [this]( boost::system::error_code e, boost::asio::ip::tcp::resolver::iterator )
            {
                if ( e ) onConnection( e );
                else Connected();
            }
        );
    }

    void Connected()
    {
        websocket.async_handshake( host, request, [this]( beast::error_code ec ){
            onConnection( ec );
        });
    }

    void Read()
    {
        beast::websocket::opcode ignoredOpcode;
        websocket.async_read(
            ignoredOpcode,
            wssb,
            [this]( boost::system::error_code ec ) { Received( ec ); }
        );
    }

    void Received( boost::system::error_code ec )
    {
#ifdef ARICPP_TRACE_WEBSOCKET
        if ( ec ) std::cerr << "*** websocket error: " << ec.message() << '\n';
        else std::cerr << "*** <== " << beast::to_string( wssb.data() ) << '\n';
#endif
        if ( ec ) onReceive( std::string(), ec );
        else onReceive( beast::to_string( wssb.data() ), ec );
        wssb.consume( wssb.size() );
        if ( ec != boost::asio::error::eof && ec != boost::asio::error::operation_aborted ) Read();
    }

    boost::asio::io_service& ios;
    const std::string host;
    const std::string port;

    boost::asio::ip::tcp::resolver resolver;
    boost::asio::ip::tcp::socket socket;
    beast::websocket::stream< boost::asio::ip::tcp::socket& > websocket;
    beast::streambuf wssb;

    std::string request;
    ConnectHandler onConnection;
    ReceiveHandler onReceive;
};

} // namespace

#endif

