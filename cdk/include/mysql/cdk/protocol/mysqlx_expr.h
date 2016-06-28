/*
 * Copyright (c) 2015, 2016, Oracle and/or its affiliates. All rights reserved.
 *
 * This code is licensed under the terms of the GPLv2
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

#ifndef MYSQL_CDK_PROTOCOL_MYSQLX_EXPR_H
#define MYSQL_CDK_PROTOCOL_MYSQLX_EXPR_H

/*
  Value, expression and document types used by the protocol API
  =============================================================

  Various methods of the protocol API use the following types to describe
  data to be put inside protocol requests:

  Any - a scalar value such as string or number or an array of Any values
        or a document in which keys are mapped to Any values.

  Expression - like Any but with base (scalar) values which can be
        expressions built using functions and operators.

  Both Expression and Any classes define inner types for scalars, arrays
  and documents:

  Any::Scalar        - base values which are plain scalars.
  Expression::Scalar - base expressions which can be built from operators etc.

  Any::List        - list of Any values.
  Expression::List - list of expressions.

  Any::Document        - document in which kets are mapped to Any values.
  Expression::Document - document in which keys are mapped to expressions.

  Also, types for lists of items are defined: Any_list and Expr_list.

*/

#include "../foundation.h"
#include "../api/expression.h"
#include "../api/document.h"

namespace cdk {
namespace protocol {
namespace mysqlx {

// These are temporary type declarations
// TODO: remove when the types are defined

typedef uint32_t stmt_id_t;
typedef uint32_t cursor_id_t;
typedef uint64_t row_count_t;
typedef uint32_t col_count_t;
// Note: protocol uses 64bit numbers for collation ids
typedef uint64_t charset_id_t;
typedef uint64_t insert_id_t;

typedef int64_t  sint64_t;
using   ::uint64_t;

}}}


namespace cdk {
namespace protocol {
namespace mysqlx {
namespace api {

using cdk::foundation::byte;
using cdk::foundation::bytes;
using cdk::foundation::string;


/*
  Any value (scalar, document or array)
  =====================================

  The class Any is defined as instantiation of generic cdk::api::Any<>
  template over a processor for base scalar values. Such base value can
  be:

  - a string represented as byte sequence with charset information (or
    without it, if default encoding is to be used).
  - a number (integer and floating)
  - Boolean value
  - raw bytes which are not treated as strings.

  As an instance of cdk::api::Any<> template, Any value can be either a
  plain scalar, an array or document. These 3 types of values ere reported
  using Any::Processor callbacks scalar(), arr() and doc(), respectively.
  See description of cdk::api::Any_processor<>.

  Any values are used for example as parameters for StmtExecute command.
*/

class Scalar_processor;

typedef cdk::api::Expr_base<Scalar_processor> Scalar;
typedef cdk::api::Expr_list<Scalar>        Scalar_list;


typedef cdk::api::Any<Scalar_processor> Any;
typedef cdk::api::Expr_list<Any>        Any_list;



class Scalar_processor
{
public:

  typedef protocol::mysqlx::charset_id_t charset_id_t;

  virtual void null() =0;

  virtual void str(bytes) =0;
  virtual void str(charset_id_t, bytes) =0;
  virtual void num(int64_t) =0;
  virtual void num(uint64_t) =0;
  virtual void num(float) =0;
  virtual void num(double) =0;
  virtual void yesno(bool) =0;
  virtual void octets(bytes) =0;
};


/*
  Expressions
  ===========

  The Expression class is defined as instantiation of generic cdk::api::Any<>
  template with a processor for expressions built from base values, variables,
  named or positional placeholders, database object references and document
  field references using operators and function application. Functions are
  either stored routines or built-in functions implemented in the server.

  Different types of expressions are described by Expr_processor callbacks.

  Since Expression is an instance of cdk::api::Any<> template, it can be
  either a plain expression, an array of expressions or a document with keys
  mapped to expressions. These three kinds of expressions are reported by
  Expression::Processor callbacks scalar(), arr() or doc(), respectively.
  See description of cdk::api::Any_processor<>.
*/


class Expr_processor;

typedef cdk::api::Any<Expr_processor>        Expression;
typedef cdk::api::Expr_list<Expression>      Expr_list;

class Doc_path;
class Db_obj;


//  Processor for expressions used in X protocol messages

class Expr_processor
{
public:

  typedef Scalar_processor     Value_prc;
  typedef api::Db_obj          Db_obj;
  typedef api::Doc_path        Doc_path;
  typedef api::Expr_list       Expr_list;
  typedef Expr_list::Processor Args_prc;

  /*
    Report expression which is a literal value. The val() callback should
    return a processor for processing the value or NULL if value should be
    ignored.
  */

  virtual Value_prc* val() =0;

  /*
    Callbacks op() and call() report an operator applied to arguments or
    a function call with arguments. In either case, the callback should
    return a list processor to process argument values (or NULL, if argument
    values are to be ignored).
  */

  virtual Args_prc* op(const char *name) =0;
  virtual Args_prc* call(const Db_obj& db_obj) =0;

  virtual void var(const string &name) =0;
  virtual void id(const string &name, const Db_obj *db_obj) =0;
  virtual void id(const string &name, const Db_obj *db_obj,
                  const Doc_path &path) =0;
  virtual void id(const Doc_path &path) =0;

  virtual void placeholder() =0;
  virtual void placeholder(const string &name) =0;
  virtual void placeholder(unsigned pos) =0;
};


/*
  Document paths
  ==============
  Interface for describing document paths (that identify fields of a document
  inside expressions).

  Let p be an object describing document path, which implements Doc_path
  interface. Path description is a sequence of N elements, where N is given by
  p.length(). The type of i-th element in the path is given by p.get_type(i).
  It can be one of:

  - MEMBER - path element which names a member of a document, the name is given
             by p.get_name(i),
  - ARRAY_INDEX - path element of the form [k] which selects k-th member of an
             array, k is given by p.get_index(),
  - MEMBER_ASTERISK - path element of the form .*, which selects all fields of
             a document.
    DOUBLE_ASTERISK - path element of the form .**, which selects all
             descendants recursively.
  - ARRAY_ASTERISK  - path element of the form [*], which selects all members of
             an array.
*/

class Doc_path
{
public:


  enum Type {
    MEMBER = 1,
    MEMBER_ASTERISK = 2,
    ARRAY_INDEX = 3,
    ARRAY_INDEX_ASTERISK = 4,
    DOUBLE_ASTERISK = 5
  };

  virtual ~Doc_path() {}

  virtual unsigned length() const =0;
  virtual Type     get_type(unsigned pos) const =0;
  virtual const string* get_name(unsigned pos) const =0;
  virtual const uint32_t* get_index(unsigned pos) const =0;
};


}  // api

}}}  // cdk::protocol::mysqlx


namespace cdk {


template<>
struct Safe_prc<protocol::mysqlx::api::Scalar_processor>
  : Safe_prc_base<protocol::mysqlx::api::Scalar_processor>
{
  typedef Safe_prc_base<protocol::mysqlx::api::Scalar_processor>  Base;
  using Base::Processor;

  typedef Processor::charset_id_t charset_id_t;

  Safe_prc(Processor *prc) : Base(prc)
  {}

  Safe_prc(Processor &prc) : Base(&prc)
  {}

  using Base::m_prc;

  void null() { return m_prc ? m_prc->null() : (void)NULL; }

  void str(bytes val)
  { return m_prc ? m_prc->str(val) : (void)NULL; }

  void str(charset_id_t cs, bytes val)
  { return m_prc ? m_prc->str(cs, val) : (void)NULL; }

  void num(int64_t val)
  { return m_prc ? m_prc->num(val) : (void)NULL; }

  void num(uint64_t val)
  { return m_prc ? m_prc->num(val) : (void)NULL; }

  void num(float val)
  { return m_prc ? m_prc->num(val) : (void)NULL; }

  void num(double val)
  { return m_prc ? m_prc->num(val) : (void)NULL; }

  void yesno(bool val)
  { return m_prc ? m_prc->yesno(val) : (void)NULL; }

  void octets(bytes data)
  { return m_prc ? m_prc->octets(data) : (void)NULL; }
};


template<>
struct Safe_prc<protocol::mysqlx::api::Expr_processor>
  : Safe_prc_base<protocol::mysqlx::api::Expr_processor>
{
  typedef Safe_prc_base<protocol::mysqlx::api::Expr_processor>  Base;
  using Base::Processor;

  typedef Processor::Value_prc   Value_prc;
  typedef Processor::Expr_list   Expr_list;
  typedef Processor::Args_prc    Args_prc;
  typedef Processor::Db_obj      Db_obj;
  typedef Processor::Doc_path    Doc_path;

  Safe_prc(Processor *prc) : Base(prc)
  {}

  Safe_prc(Processor &prc) : Base(&prc)
  {}

  using Base::m_prc;

  Safe_prc<Value_prc>  val()
  { return m_prc ? m_prc->val() : NULL; }

  Safe_prc<Args_prc>   op(const char *name)
  { return m_prc ? m_prc->op(name) : NULL; }

  Safe_prc<Args_prc>   call(const Db_obj &func)
  { return m_prc ? m_prc->call(func) : NULL; }

  void var(const string &name)
  { return m_prc ? m_prc->var(name) : (void)NULL; }

  void id(const string &name, const Db_obj *db_obj)
  { return m_prc ? m_prc->id(name, db_obj) : (void)NULL; }

  void id(const string &name, const Db_obj *db_obj, const Doc_path &path)
  { return m_prc ? m_prc->id(name, db_obj, path) : (void)NULL; }

  void id(const Doc_path &path)
  { return m_prc ? m_prc->id(path) : (void)NULL; }

  void placeholder()
  { return m_prc ? m_prc->placeholder() : (void)NULL; }

  void placeholder(const string &name)
  { return m_prc ? m_prc->placeholder(name) : (void)NULL; }

  void placeholder(unsigned pos)
  { return m_prc ? m_prc->placeholder(pos) : (void)NULL; }
};


}
#endif
