/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * The MySQL Connector/C++ is licensed under the terms of the GPLv2
 * <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>, like most
 * MySQL Connectors. There are special exceptions to the terms and
 * conditions of the GPLv2 as it is applied to this software, see the
 * FLOSS License Exception
 * <http://www.mysql.com/about/legal/licensing/foss-exception.html>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */


#ifndef PROTOCOL_MYSQLX_PROTOCOL_H
#define PROTOCOL_MYSQLX_PROTOCOL_H

#include <mysql/cdk/protocol/mysqlx.h>
#include <mysql/cdk/foundation/opaque_impl.i>
#include <mysql/cdk/config.h>


PUSH_PB_WARNINGS
#include "protobuf/mysqlx.pb.h"
POP_PB_WARNINGS


namespace google {
namespace protobuf {

class Message;
class MessageLite;

}} // google::protobuf



namespace cdk {
namespace protocol {
namespace mysqlx {

#ifdef DEBUG_PROTOBUF
using google::protobuf::Message;
#else
typedef google::protobuf::MessageLite Message;
#endif

typedef uint32_t            msg_size_t;
typedef unsigned short int  msg_type_t;


// Convert value stored in variable S of type msg_size_t from
// byte-order used on the cable (little-endian) to the host
// byte order and vice-versa.

#if defined(HAVE_ENDIAN_H)
  #include <sys/endian.h>
#elif defined(HAVE_BYTEORDER_H)
  #include <sys/byteorder.h>
  #define bswap32(X) BSWAP_32(X)
#elif BIG_ENDIAN
  #error No byte-swap function available on big-endian platform.
#endif

#if BIG_ENDIAN
#define NTOHSIZE(S) do { (S) = bswap32(S); } while(0)
#define HTONSIZE(S) do { (S) = bswap32(S); } while(0)
#else
#define NTOHSIZE(S)
#define HTONSIZE(S)
#endif


///  Length of mysqlx message header.
const size_t header_length= 5;

/// Maximum size of internal buffer used to send or receive messages.
const size_t max_wr_size= 1024*1024*1024;  // 1GB
const size_t max_rd_size= max_wr_size;

// TODO: use throw_error or any other appropriate method when the code is ready
#define THROW_PROTOCOL_ERROR(ERR) throw ERR



enum Protocol_side { SERVER, CLIENT};

inline
Protocol_side other_side(Protocol_side side)
{
  switch (side)
  {
  case SERVER: return CLIENT;
  case CLIENT: return SERVER;
  default: THROW("unknown protocol side");
  }
}


class Op_base;
class Op_rcv;

/*
  Internal implementation for Protocol class.

*/

class Protocol_impl : public Processor_base
{
public:

  Protocol::Stream *m_str;
  /// The side from which we *receive* messages
  Protocol_side m_side;

protected:

  Protocol_impl(Protocol::Stream*, Protocol_side);
  virtual ~Protocol_impl();

public:

  /**
    Start async op that sends given message to the other end.

    Returns (reference to) an object representing this asynchronous
    operation.
  */

  virtual Protocol::Op& snd_start(Message &msg, msg_type_t msg_type);

  /**
    Start (next stage of) an async op that processes incoming message(s).

    Asynchronous processing of incoming messages can be done in stages.
    Single receive operation started by this method completes one stage of
    the processing. After this, processing operation is stopped and waits
    to be resumed with another call to rcv_start() which will start
    next stage of processing.

    PRC is the type of processor object that will be used in this stage
    of processing.

    RCV is the type of the object that implements message processing
    operation. If no such operation is ongoing at the moment, new one will
    be created and its first stage will be started. Otherwise, if there
    is ongoing processing operation, it is assumed that it is of type RCV
    and it is resumed using given processor.

    Returns (reference to) an object representing next stage of processing
    operation.
  */

  template <class RCV, class PRC>
  Protocol::Op& rcv_start(PRC&);

protected:

  /*
    Reading raw message frames
    --------------------------

    Method read_header() starts asynchronous reading of message frame header.
    If header was already read, it does nothing. Information extracted from
    the header is stored in m_msg_type and m_msg_size members. This method can
    be called only at the beginning or after reading message payload.

    Method read_payload() starts asynchronous reading of message payload.
    If payload has been already read, it does nothing. The payload is placed
    in m_rd_buf buffer. This method can be called only after reading message
    header.

    To complete the asynchronous header/payload reading operation one has
    to call method rd_cont() until it returns true.
  */

  enum { HEADER, PAYLOAD }   m_msg_state;

  void read_header();
  void read_payload();
  bool rd_cont();
  void rd_wait();

  byte   *m_rd_buf;
  size_t  m_rd_size;
  scoped_ptr<Protocol::Stream::Op> m_rd_op;

  // Info extracted from message header

  msg_type_t m_msg_type;
  size_t     m_msg_size;

  /*
    Writing raw message frames
    --------------------------

    Method write_msg() starts asynchronous operation which serializes given
    message and sends it to the other end after wrapping in correct message
    frame.

    To complete writing operation one has to call method wr_cont() until it
    returns true.
  */

  void write_msg(msg_type_t, Message&);
  bool wr_cont();
  void wr_wait();

  byte   *m_wr_buf;
  size_t  m_wr_size;
  scoped_ptr<Protocol::Stream::Op> m_wr_op;

  bool resize_buf(Protocol_side side, size_t new_size);

public:

  /**
    Extension of asynchronous operation interface which is used by multi
    stage operations (such as ones used to process incoming messages).

    For such operation, is_completed() returns true when the current stage
    is completed. New method is_done() informs if the whole operation is
    done.
  */

  class Op : public Protocol::Op
  {
  public:
    virtual bool is_done() const =0;
  };

private:
  void rd_process();

  // Pointers to the current send/receive operations
  scoped_ptr<Op> m_snd_op;
  scoped_ptr<Op> m_rcv_op;

  friend class Op_base;
  friend class Op_rcv;
  friend class Op_snd;
};


template <class Rcv, class Prc>
inline
Protocol::Op& Protocol_impl::rcv_start(Prc &prc)
{
  // If last receive operation is done, remove it first.

  if (m_rcv_op && m_rcv_op->is_done())
    m_rcv_op.reset();

  // Create new receive operation if none is active at the moment.

  if (!m_rcv_op)
    m_rcv_op.reset(new Rcv(*this));

  // Resume the operation starting its new stage.
  // TODO: operation type check

  static_cast<Rcv*>(m_rcv_op.get())->resume(prc);
  return *m_rcv_op;
}



/*
  Specializations of Protocol_impl for client and server-side
*/


class Protocol::Impl
  : public Protocol_impl
{
public:

  Impl(Protocol::Stream *str)
    : Protocol_impl(str, SERVER)  // expects messages from server
  {}
};


class Protocol_server::Impl
  : public Protocol_impl
{
public:

  Impl(Protocol::Stream *str)
    : Protocol_impl(str, CLIENT)  // expects messages from client
  {}
};


/*
  Asynchronous send and receive operations used by protocol
  implementation
  =========================================================
*/


/**
  Base for protocol asynchronous operations.

  It stores reference to protocol implementation object which is used
  to drive low-level reading/writing of message frames. It also contains
  infrastructure for storing errors detected while performing the operation
  if reporting of these errors should be deferred till later.
*/

class Op_base : public Protocol_impl::Op
{
protected:

  Protocol_impl &m_proto;
  bool m_completed;

  Op_base(Protocol_impl &proto)
    : m_proto(proto)
    , m_completed(false)
  {}

  bool is_done() const { return is_completed(); }

  // Async_op

  bool is_completed() const { return m_completed; }

  void do_cancel() { THROW("not implemented"); }

  cdk::api::Event_info* get_event_info() const { return NULL; }

protected:

  scoped_ptr<Error>  m_error;

  /**
    Save arbitrary exception wrapped in a CDK Error as m_error.
    To be used inside catch() block.
  */
  void save_error()
  {
    try { rethrow_error(); }
    catch (Error &e)
    {
      m_error.reset(e.clone());
    }
  }

  /**
    Throw error saved in m_error. Does nothing if no error was saved.
  */
  void throw_saved_error()
  {
    if (m_error)
      m_error->rethrow();
  }

};


/**
  Message sending operation.

  The work is done by protocol instance - this is just a simple wrapper.
*/

class Op_snd : public Op_base
{
public:

  Op_snd(Protocol_impl &proto, msg_type_t type, Message &msg)
    : Op_base(proto)
  {
    m_proto.write_msg(type, msg);
  }

  bool do_cont()
  {
    if (!m_proto.wr_cont())
      return false;
    m_completed= true;
    return true;
  }

  void do_wait()
  {
    m_proto.wr_wait();
    m_completed = true;
  }

  size_t do_get_result()
  { THROW("not implemented"); }
};


/**
  Operation that processes incoming messages.

  This class provides common framework for processing. It reads message
  header and payload using the protocol instance. It also makes Processor_base
  callbacks informing about message boundaries and possibly passing the
  payload to the processor.

  The actual processing of message payload is done by do_process_msg()
  method which should be overridden by derived classes.

  Controlling message flow
  ------------------------
  Receive operation can read one or more messages from the server and it
  can stop after seeing a message header. This is controlled by the following
  factors:

  - method next_msg() - after reading message header tells if this message
                        type was expected and whether to continue with
                        processing its payload or stop here.

  - method process_next() - after processing one message, tells whether to
                            read the next one or stop processing here.

  - processor callback message_end() - can request to stop processing further
                            messages.

  By default, next_msg() treats server notices and errors as expected messages
  and for others it calls do_next_msg() to determine if they are expected
  (default behavior is to treat other messages as unexpected). Derived
  classes should override do_next_msg() to enable processing of other message
  types.

  Method process_next() uses the following logic:

  - request reading another message after seeing a notice,
  - stop reading messages after seeing an error,
  - for other message types, call do_process_next() to determine whether
    another message should be received.

  By default reading messages stops after processing one message (with
  exception of notices). Derived classes should override do_process_next()
  to process more messages in single receive operation.

  Processing message payload
  --------------------------
  This is done in process_msg() method. By default it processes errors
  and notices, passing information to the processor, and for other message
  types it calls do_process_msg() which should be overridden by derived
  classes.

  Note that calling base processor methods such as message_begin/end() and
  also passing raw message bytes to the processor if requested is already
  handeld by Rcv_op class.. Derived classes need to implement only passing
  to the processor information that is specific to each supported message
  type.

*/

class Op_rcv : public Op_base
{
protected:

  enum { HEADER, PAYLOAD, DONE } m_stage;
  Processor_base *m_prc;

public:

  Op_rcv(Protocol_impl &proto)
    : Op_base(proto)
    , m_stage(HEADER)
    , m_prc(NULL)
  {}

  /**
    This method should be called to start reading and processing
    single message using given message processor.
  */

  void read_msg(Processor_base &prc)
  {
    m_prc= &prc;
    m_proto.read_header();
    m_stage= HEADER;
  }

  /*
    Start processing single message using the same message processor
    as last time.
  */

  void read_msg()
  {
    assert(m_prc);
    read_msg(*m_prc);
  }

protected:

  msg_type_t  m_msg_type;

  /*
    Methods controlling processing of messages that can be overridden
    by specializations.
  */

  /*
    Method next_msg() is called after reading message header to determine
    if given message is expected. It can also tell the operation to stop
    here without processing given message, which would be picked up by when
    operation is resumed (or by next operation).

    Method next_msg() treats Error and Notice messages as expected and for
    others it calls do_next_msg().
  */

  enum Next_msg { EXPECTED, UNEXPECTED, STOP };
  virtual Next_msg do_next_msg(msg_type_t) { return UNEXPECTED; }

  virtual Next_msg next_msg(msg_type_t type)
  {
    switch (type)
    {
      // TODO: Notice
    case msg_type::Error: return EXPECTED;
    case msg_type::Notice: return EXPECTED;
    default: return do_next_msg(type);
    }
  }

  /*
    Process parsed message. Specializations should override to pass
    information from message to the processor.

    Method process_msg() handles Error and Notice messages and delegates
    others to do_process_msg().
  */

  virtual void process_msg(msg_type_t, Message&);
  virtual void do_process_msg(msg_type_t, Message&) {}

  /**
    This method is called after processing each message to determine
    if operation should continue processing next message or stop.

    By default, after Notice next message is processed. Otherwise
    do_prcess_next() determines if another message should be read.
  */

  virtual bool process_next()
  {
    if (msg_type::Notice == m_msg_type)
      return true;
    else if (msg_type::Error == m_msg_type)
      return false;
    else
      return do_process_next();
  }

  virtual bool do_process_next() { return false; }

private:

  size_t      m_msg_size;
  size_t      m_read_window;

  bool   m_call_message_end;
  bool   m_skip;

  void process_payload();
  bool finish(bool stop = false);

  // Async_op

  bool do_cont();
  void do_wait();

  // Method implementing main message reading logic.

  bool do_read_msg(bool wait);

  size_t do_get_result()
  { THROW("not implemented"); }

};


inline
void Op_rcv::process_msg(msg_type_t type, Message &msg)
{

  if (msg_type::Notice == m_msg_type)
  {
    Error_processor &ep= static_cast<Error_processor&>(*m_prc);
    Mysqlx::Notice::Frame   &notice= static_cast<Mysqlx::Notice::Frame&>(msg);
    ep.notice(notice.type(),
              (short)notice.scope(),
              bytes((byte*)notice.payload().c_str(),
                    (size_t)notice.payload().length()));
    return;
  }
  else if (msg_type::Error == m_msg_type)
  {
    Error_processor &ep= static_cast<Error_processor&>(*m_prc);
    Mysqlx::Error   &err= static_cast<Mysqlx::Error&>(msg);
    sql_state_t sqlstate(err.sql_state());
    /*
      There are 2 error severities: 0 = ERROR, 1 = FATAL. For us both
      are treated as 2 = ERROR.
    */
    ep.error(err.code(), 2, sqlstate, err.msg());
    return;
  }

  do_process_msg(type, msg);
}


/*
  Helper template which dispatches process_msg_with(type, msg, processor) call
  to call of method generated from process_msg_with<MSG,PRC>() template, where
  MSG is the protobuf type of the message and PRC is type of the processor.
*/

template <class Base>
class Message_dispatcher: public Base
{
public:

  typedef Message_dispatcher<Base> Dispatcher;

  Message_dispatcher(Protocol_impl &proto) : Base(proto)
  {}

  template <class RPC>
  void process_msg_with(msg_type_t, Message&, RPC&);

};



template <class Base>
template <class PRC>
inline
void Message_dispatcher<Base>::process_msg_with(msg_type_t type, Message &msg, PRC &prc)
{

#define MSG_CLIENT_PRC_CLI(MSG,N,C) \
  case msg_type::cli_##N: Base::process_msg_with(static_cast<MSG&>(msg), prc); break;
#define MSG_SERVER_PRC_CLI(MSG,N,C)
#define MSG_SERVER_PRC_SRV(MSG,N,C) \
  case msg_type::N: Base::process_msg_with(static_cast<MSG&>(msg), prc); break;
#define MSG_CLIENT_PRC_SRV(MSG,N,C)

  switch (Base::m_proto.m_side)
  {
  case SERVER:
    switch (type)
    {
      MSG_LIST(PRC_SRV)
      // TODO: proper error
      default: THROW("unknown server message type");
    };
    break;

  case CLIENT:
    switch (type)
    {
      MSG_LIST(PRC_CLI)
      default: THROW("unknown server message type");
    };
    break;

  default: THROW("unknown protocol side");
  }

}

#if 0
class Expr_processor : public api::Expression::Processor
{
private:
  Mysqlx::Expr::Expr& m_expr;
  Mysqlx::Expr::ColumnIdentifier *alloc_init_id(const string *name,
                                                const api::Db_obj *coll);
  Mysqlx::Datatypes::Scalar *mk_literal(Mysqlx::Datatypes::Scalar_Type type)
  {
    m_expr.set_type(Mysqlx::Expr::Expr_Type_LITERAL);
    return *m_expr.mutable_literal();
    Mysqlx::Datatypes::Scalar *s= m_expr.mutable_literal();
    s->set_type(type);
    return s;
  }

public:

  Expr_processor(Mysqlx::Expr::Expr &expr) : m_expr(expr)
  {}

  virtual void null();

  virtual void str(bytes);
  virtual void str(charset_id_t /*charset*/, bytes);
  virtual void num(int64_t);
  virtual void num(uint64_t);
  virtual void num(float);
  virtual void num(double);
  virtual void yesno(bool);
  virtual void octets(bytes);

  virtual void var(const string &name);
  virtual void id(const string &name, const api::Db_obj *coll);
  virtual void id(const string &name, const api::Db_obj *coll,
                  const api::Doc_path &path);
  virtual void id(const api::Doc_path &path);
  virtual void op(const char *name, const api::Expr_list &args);
  virtual void call(const api::Db_obj& db_obj, const api::Expr_list &args);

  virtual void placeholder();
  virtual void placeholder(const string &name);
  virtual void placeholder(unsigned pos);

  virtual void arr(const Expr_list&);
  virtual void doc(const Document&);
};
#endif

}}}

IMPL_TYPE(cdk::protocol::mysqlx::Protocol, cdk::protocol::mysqlx::Protocol::Impl);
IMPL_TYPE(cdk::protocol::mysqlx::Protocol_server, cdk::protocol::mysqlx::Protocol_server::Impl);


#endif
