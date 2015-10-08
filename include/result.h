#ifndef MYSQLX_RESULT_H
#define MYSQLX_RESULT_H

/**
  @file
  Classes used to access query and command execution results.
*/


#include "common.h"
#include "document.h"


namespace cdk {

  class Reply;

}  // cdk


namespace mysqlx {

using std::ostream;

class Session;
class Schema;
class Collection;
class Result;
class Row;
class RowResult;
class DbDoc;
class DocResult;

class Task;
class Executable;


class BaseResult : nocopy
{
  class Impl;
  Impl  *m_impl;
  bool m_owns_impl;
  row_count_t  m_pos;

  BaseResult(cdk::Reply*);
  BaseResult(cdk::Reply*, const GUID&);

protected:

  BaseResult()
    : m_impl(NULL), m_owns_impl(true)
    , m_pos(0)
  {}

  BaseResult& operator=(BaseResult &&other)
  {
    init(std::move(other));
    return *this;
  }

  void init(BaseResult&&);

public:

  BaseResult(BaseResult &&other) { init(std::move(other)); }
  virtual ~BaseResult();

  friend class NodeSession;
  friend class Result;
  friend class RowResult;
  friend class DocResult;
  friend class Task;

  struct Access;
  friend struct Access;
};

inline
void BaseResult::init(BaseResult &&init)
{
  m_pos = 0;
  m_impl = init.m_impl;
  if (!init.m_owns_impl)
    m_owns_impl = false;
  else
  {
    m_owns_impl = true;
    init.m_owns_impl = false;
  }
}


/**
  Represents result of an operation that does not return data.

  Generic result which can be returned by operations which only
  modify data.

  `Result` instance can store result of executing an operation:

  ~~~~~~
  Result res = operation.execute();
  ~~~~~~

  Storing another result in `Result` instance will overwrite
  previous result.

  @todo Implement other methods for getting information about
  the result specified by DevAPI.
*/

class Result : public BaseResult
{
public:

  /**
    Construct empty `Result` instance that can
    store results returned by `execute()` method.
  */

  Result& operator=(BaseResult &&other)
  {
    init(std::move(other));
    return *this;
  }

  /**
    Return id of the last document which the operation
    added to a collection.
  */

  const GUID& getLastDocumentId() const;

};



// Row based results
// -----------------


/**
  Represents a single row from a result that contains rows.

  Such a row consists of a number of fields, each storing single
  value. The number of fields and types of values stored in each
  field are described by `RowResult` instance that produced this
  row.

  Values of fields can be accessed with `get()` method or using
  `row[pos]` expression. Fields are identified by 0-based position.
  It is also possible to get raw bytes representing value of a
  given field with `getBytes()` method.
  
  @sa `Value` class.
  @todo Support for iterating over row fields with range-for loop.
*/

class Row : nocopy
{
public:

  virtual ~Row() {}

  virtual const string getString(col_count_t pos) =0;

  /// Get raw bytes representing value of row field at position `pos`.
  virtual bytes getBytes(col_count_t pos) =0;

  /// Get value of row field at position `pos`.
  virtual Value get(col_count_t) = 0;

  /// Convenience operator equivalent to `get()`.
  const Value operator[](col_count_t pos)
  { return get(pos); }
};


/**
  %Result of an operation that returns rows.
*/

class RowResult : public BaseResult
{

public:

  /*
    Note: Even though we have RowResult(Result&&) constructor below,
    we still need move-ctor for such copy-initialization to work:

      RowResult res= coll...execute();

    This copy-initialization works as follows 
    (see http://en.cppreference.com/w/cpp/language/copy_initialization):

    1. A temporary prvalue of type RowResult is created by type-conversion
       of the Result prvalue coll...execute(). Constructor RowResult(Result&&)
       is calld to do the conversion.

    2. Now res is direct-initialized 
       (http://en.cppreference.com/w/cpp/language/direct_initialization)
       from the prvalue produced in step 1.

    Since RowResult has disabled copy constructor, a move constructor is
    required for direct-initialization in step 2. Even though move-constructor
    is actually not called (because of copy-elision), it must be declared
    in the RowResult class. We also define it for the case that copy-elision
    was not applied.
  */

  RowResult(RowResult &&other)
    : BaseResult(std::move(static_cast<BaseResult&>(other)))
  {}

  RowResult(BaseResult &&init)
    : BaseResult(std::move(init))
  {}

  /// Retrun number of fields in each row.
  col_count_t getColumnCount() const;

  /**
    Return current row and move to the next one in the sequence.

    If there are no more rows in this result, returns NULL.
  */

  Row* fetchOne();

  friend class Task;
};


// Document based results
// ----------------------


/**
  %Result of an operation that returns documents.
*/

class DocResult : public BaseResult
{
  class Impl;
  Impl *m_doc_impl;

public:

  DocResult(DocResult &&other)
    : m_doc_impl(NULL)
  {
    *this = std::move(static_cast<BaseResult&>(other));
  }

  DocResult(BaseResult &&init)
    : m_doc_impl(NULL)
  {
    *this = std::move(init);
  }

  virtual ~DocResult();

  void operator=(BaseResult &&init);

  /**
    Return current document and move to the next one in the sequence.

    If there are no more documents in this result returns NULL.
  */

  DbDoc* fetchOne();

  friend class Impl;
  friend class Task;
  friend class DbDoc;
};


}  // mysqlx

#endif
