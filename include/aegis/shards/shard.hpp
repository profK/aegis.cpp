//
// shard.hpp
// *********
//
// Copyright (c) 2018 Sharon W (sharon at aegis dot gg)
//
// Distributed under the MIT License. (See accompanying file LICENSE)
//

#pragma once

#include "aegis/config.hpp"
#include "aegis/utility.hpp"
#include "aegis/fwd.hpp"
#include <memory>
#include <map>
#include <string>
#include <chrono>
#include <stdint.h>
#include <asio/io_context.hpp>
#ifdef WIN32
# include "aegis/push.hpp"
#endif
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/common/connection_hdl.hpp>
#include <websocketpp/client.hpp>
#ifdef WIN32
# include "aegis/pop.hpp"
#endif
#include <spdlog/fmt/fmt.h>
#include "aegis/zstr/zstr.hpp"

namespace aegis
{

namespace shards
{

/// Tracks websocket shards and their connections
class shard
{
public:
    /// Type of a pointer to the Websocket++ TLS connection
    using connection_ptr = websocketpp::client<websocketpp::config::asio_tls_client>::connection_type::ptr;

    /// Constructs a shard object for connecting to the websocket gateway and tracking timers
    AEGIS_DECL shard(asio::io_context & _io, websocketpp::client<websocketpp::config::asio_tls_client> & _ws, int32_t id);

    /// Resets connection, heartbeat, and timer related objects to allow reconnection
    AEGIS_DECL void do_reset(shard_status _status = shard_status::Closed) noexcept;

    /// Get this shard's websocket message sequence counter
    /**
     * @returns Sequence counter specific to this shard
     */
    int64_t get_sequence() const noexcept
    {
        return _sequence;
    }

    /// Gets the id of the shard in the master list
    /**
     * @see core::shards
     * @returns Shard id
     */
    int32_t get_id() const noexcept
    {
        return _id;
    }

    /// Check if this shard has an active connection or is a connecting state
    /**
     * @returns bool
     */
    AEGIS_DECL bool is_connected() const noexcept;

    /// Check if this shard is ready to interact with the gateway
    /**
     * @returns bool
     */
    AEGIS_DECL bool is_online() const noexcept;

    /// Send a message to this shard's websocket connection asynchronously
    /**
     * @param payload String of the payload to send
     * @param op Opcode of the message (default: text)
     */
    AEGIS_DECL void send(const std::string & payload, websocketpp::frame::opcode::value op = websocketpp::frame::opcode::text);

    /// Send message over the websocket synchronously 
    AEGIS_DECL void send_now(const std::string & payload, websocketpp::frame::opcode::value op = websocketpp::frame::opcode::text);

    /// Returns a formatted string of bytes received since library start
    /**
     * @returns std::string of the current bytes received since start
     */
    std::string get_transfer_str() const noexcept
    {
        return utility::format_bytes(transfer_bytes);
    }

    /// Returns a formatted string of uncompressed bytes received since library start
    /**
     * Uncompressed as in post-inflate. This is the amount of bytes that would have been
     * transferred without compression. To be compared with get_transfer_str() for a compression ratio.
     * @returns std::string of the current bytes received since start
     */
    std::string get_transfer_u_str() const noexcept
    {
        return utility::format_bytes(transfer_bytes_u);
    }

    /// Returns bytes transferred pre-inflation used to with post-inflation for
    /// bandwidth saving/efficiency comparisons
    /**
     * @returns uint64_t of the current bytes received since start
     */
    uint64_t get_transfer() const noexcept
    {
        return transfer_bytes;
    }

    /// Returns bytes transferred post-inflation used to with pre-inflation for
    /// bandwidth saving/efficiency comparisons
    /**
     * @returns uint64_t of the current bytes received since start
     */
    uint64_t get_transfer_u() const noexcept
    {
        return transfer_bytes_u;
    }

    /// Contains counters of valued objects and events
    struct
    {
        int64_t messages;
        int64_t presence_changes;
        int64_t reconnects;
    } counters = { 0,0,0 };

    /// Open websocket connection
    AEGIS_DECL void connect();

    /// Return shard uptime as {days hours minutes seconds}
    /**
     * @returns std::string of `hh mm ss` formatted time
     */
    AEGIS_DECL std::string uptime_str() const noexcept
    {
        return utility::uptime_str(_ready_time);
    }

    /// Return shard uptime as {days hours minutes seconds}
    /**
     * @returns Time in milliseconds since shard received ready
     */
    int64_t uptime() const noexcept
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - _ready_time).count();
    }

    void set_heartbeat(std::function<void(const asio::error_code &, const std::chrono::milliseconds, shard *)> fnkeepalive)
    {
        keepalivefunc = fnkeepalive;
    }

    AEGIS_DECL void start_heartbeat(int32_t heartbeat) noexcept;

    std::deque<std::tuple<int64_t, std::string>> debug_messages;
    asio::steady_timer keepalivetimer;
    asio::steady_timer delayedauth;
    asio::steady_timer write_timer;

    std::queue<std::tuple<std::string, websocketpp::frame::opcode::value>> write_queue;

    int32_t heartbeattime;

    std::chrono::time_point<std::chrono::steady_clock> heartbeat_ack;
    std::chrono::time_point<std::chrono::steady_clock> lastheartbeat;
    std::chrono::time_point<std::chrono::steady_clock> lastwsevent;
    std::chrono::time_point<std::chrono::steady_clock> last_status_time;
    std::chrono::time_point<std::chrono::steady_clock> _ready_time;
    std::chrono::time_point<std::chrono::steady_clock> last_ws_write;
    std::chrono::time_point<std::chrono::steady_clock> connect_time;

    shard_status connection_state;
    std::string session_id;
    std::function<void(const asio::error_code &, const std::chrono::milliseconds, shard *)> keepalivefunc;

    void set_sequence(int64_t seq) noexcept
    {
        _sequence = seq;
    }

    void set_id(int32_t shard_id) noexcept
    {
        _id = shard_id;
    }

    connection_ptr get_connection() noexcept
    {
        return _connection;
    }

    std::vector<std::string> get_trace() const noexcept
    {
        return _trace;
    }

private:
    friend class shard_mgr;
    friend class aegis::core;

    AEGIS_DECL void process_writes(const asio::error_code & ec);
    AEGIS_DECL void _reset();
    AEGIS_DECL void set_connected();

    connection_ptr _connection;

    int64_t _sequence;
    int32_t _id;

    asio::io_context & _io_context;

    /// bytes transferred
    uint64_t transfer_bytes;

    /// bytes transferred without compression
    uint64_t transfer_bytes_u;

    websocketpp::client<websocketpp::config::asio_tls_client> & _websocket;

    std::stringstream ws_buffer;
    std::unique_ptr<zstr::istream> zlib_ctx;

    // Websocket++ socket connection
    websocketpp::connection_hdl hdl;
    std::vector<std::string> _trace;
};

}

}

#if defined(AEGIS_HEADER_ONLY)
#include "aegis/shards/impl/shard.cpp"
#endif
