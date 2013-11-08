// Copyright (C) 2013 Vrije Universiteit Brussel, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.


#ifndef cf3_lss_detail_matrix_hpp
#define cf3_lss_detail_matrix_hpp


#include <iterator>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "index.hpp"
#include "utilities.hpp"


namespace cf3 {
namespace lss {
namespace detail {


/* -- matrix interface and implementations ---------------------------------- */


// utilities
enum orientation_t { column_oriented=0, row_oriented=1 };
enum print_t { print_auto, print_size, print_signs, print_full };
print_t print_level(const int& i);


/**
 * @brief Generic matrix basic interface, based on class template implementation
 * (like profile pattern) to implement functionality (this is NOT polymorphism).
 * T: storage type
 * Impl: implementation type
 */
template< typename T, class IMPL >
struct matrix
{
  // constructor
  matrix() :
    m_zero(std::numeric_limits< T >::quiet_NaN()),
    m_size(),
    m_print(print_auto)
  {}

  // -- functionality in remote implementation

  matrix& initialize(const size_t& i, const size_t& j, const double& _value=double()) { return IMPL::initialize(i,j,_value); }
  matrix& initialize(const std::vector< double >& _vector) { return IMPL::initialize(_vector); }
  matrix& initialize(const std::string& _fname)            { return IMPL::initialize(_fname); }

  matrix& clear()                         { return IMPL::initialize(m_size.i,m_size.j,double()); }
  matrix& operator=(const double& _value) { return IMPL::initialize(m_size.i,m_size.j,_value); }
  matrix& operator=(const matrix& _other) { return IMPL::operator=(_other); }
  matrix& zerorow(const size_t& i)        { return IMPL::zerorow(i); }

  // -- intrinsic functionality

  void swap(matrix& other) {
    std::swap(other.m_size,m_size);
    std::swap(other.m_zero,m_zero);
    std::swap(other.m_print,m_print);
  }

  virtual std::ostream& print(std::ostream& o) const {
    const T eps = 1.e3*std::numeric_limits< T >::epsilon();
    const print_t print_level(m_print? m_print :
                             (m_size.i>100 || m_size.j>100? print_size  :
                             (m_size.i> 10 || m_size.j> 10? print_signs :
                                                            print_full )));
    o << "(" << size(0) << 'x' << size(1) << ") [ ";
    if      (print_level==print_size)  { o << "..."; }
    else if (print_level==print_signs) {
      for (size_t i=0; i<size(0); ++i) {
        std::string str(size(1),'0');
        for (size_t j=0; j<size(1); ++j)
          str[j] = (operator()(i,j)>eps? '+' : (operator()(i,j)<-eps? '-':'0'));
        o << "\n  " << str;
      }
    }
    else if (print_level==print_full) {
      for (size_t i=0; i<size(0); ++i) {
        std::ostringstream ss;
        for (size_t j=0; j<size(1); ++j)
          ss << operator()(i,j) << ", ";
        o << "\n  " << ss.str();
      }
    }
    return o << " ]";
  }

  size_t size(const size_t& _d) const {
    return (_d==0? m_size.i :
           (_d==1? m_size.j : std::numeric_limits< size_t >::max()));
  }
  const idx_t& size() const { return m_size; }

  // indexing (defer to implementation)
  virtual const T& operator()(const size_t& i, const size_t& j=0) const = 0;
  virtual       T& operator()(const size_t& i, const size_t& j=0)       = 0;

  // members (implementation should maintain this)
  T m_zero;
  idx_t m_size;
  print_t m_print;
};


/* -- matrix implementations ------------------------------------------------ */


/**
 * @brief Dense matrix, stored in row or column oriented vector< vector< T > >
 * (can work as column vectors if that is the intention, and sometimes it is...)
 * T: storage type
 * ORIENT: if storage is row (default) or column oriented
 *
 * FIXME: I will remove this, there is no need to keep this structure!
 */
template< typename T, int ORIENT=row_oriented >
struct dense_matrix_vv :
    matrix< T, dense_matrix_vv< T > >
{
  typedef matrix< T, dense_matrix_vv< T > > matrix_base_t;
  using matrix_base_t::size;

  dense_matrix_vv& initialize(const size_t& i, const size_t& j, const double& _value=double()) {
    if (idx_t(i,j).is_valid_size()) {
      clear();
      matrix_base_t::m_size = idx_t(i,j);
      if (size(0)*size(1))
        a.assign(ORIENT? size(0):size(1),std::vector< T >(
                 ORIENT? size(1):size(0),static_cast< T >(_value) ));
    }
    return *this;
  }

  dense_matrix_vv& initialize(const std::vector< double >& _vector) {
    if (size(0)*size(1)!=_vector.size())
      throw std::runtime_error("dense_matrix_vv: assignment not consistent with current size.");
    for (size_t i=0, k=0; i<size(0); ++i)
      for (size_t j=0; j<size(1); ++j, ++k)
        operator()(i,j) = (type_is_equal< T, double >()? (const T) _vector[k]
                                                       : static_cast< const T >(_vector[k]) );
    return *this;
  }

  dense_matrix_vv& initialize(const std::string& _fname) {
    clear();
    try { read_dense< T >(_fname,ORIENT,matrix_base_t::m_size,a); }
    catch (const std::runtime_error& e) {
      throw std::runtime_error("dense_matrix_vv: " + std::string(e.what()) + " cannot read file.");
    }
    return *this;
  }

  dense_matrix_vv& clear() {
    a.clear();
    matrix_base_t::m_size.clear();
    return *this;
  }

  dense_matrix_vv& operator=(const dense_matrix_vv& _other) {
    a = _other.a;
    matrix_base_t::m_size = _other.matrix_base_t::m_size;
    return *this;
  }

  dense_matrix_vv& zerorow(const size_t& i) {
    if (i>=size(0))
      throw std::runtime_error("dense_matrix_vv: row index outside bounds.");
    if (ORIENT) a[i].assign(size(1),T());
    else {
      for (size_t j=0; j<size(1); ++j)
        a[j][i] = T();
    }
    return *this;
  }

  dense_matrix_vv& swap(dense_matrix_vv& other) {
    other.a.swap(a);
    matrix_base_t::swap(other);
    return *this;
  }

  // indexing
  const T& operator()(const size_t& i, const size_t& j=0) const { return ORIENT? a[i][j]:a[j][i]; }
        T& operator()(const size_t& i, const size_t& j=0)       { return ORIENT? a[i][j]:a[j][i]; }

  // storage
  std::vector< std::vector< T > > a;
};


/**
 * @brief Dense matrix, stored in row or column oriented vector< vector< T > >
 * (can work as column vectors if that is the intention, and sometimes it is...)
 * T: storage type
 * ORIENT: if storage is row (default) or column oriented
 */
template< typename T, int ORIENT=row_oriented >
struct dense_matrix_v :
    matrix< T, dense_matrix_v< T > >
{
  typedef matrix< T, dense_matrix_v< T > > matrix_base_t;
  using matrix_base_t::size;

  // initializations

  dense_matrix_v& initialize(const size_t& i, const size_t& j, const double& _value=double()) {
    if (idx_t(i,j).is_valid_size()) {
      clear();
      matrix_base_t::m_size = idx_t(i,j);
      if (size(0)*size(1))
        a.assign(size(0)*size(1),static_cast< T >(_value));
    }
    return *this;
  }

  dense_matrix_v& initialize(const std::vector< double >& _vector) {
    if (a.size()!=_vector.size())
      throw std::runtime_error("dense_matrix_v: assignment not consistent with current size.");

    std::vector< T > w;
    if (!type_is_equal< T, double >()) {
      w.resize(_vector.size());
      std::transform(_vector.begin(),_vector.end(),w.begin(),type_conversion_t< double, T >() );
    }
    const std::vector< T >& v(type_is_equal< T, double >()? (const std::vector< T >&) _vector : w);

    if (ORIENT) { a = v; }
    else {
      for (size_t i=0, k=0; i<size(0); ++i)
        for (size_t j=0; j<size(1); ++j, ++k)
          operator()(i,j) = v[k];
    }
    return *this;
  }

  dense_matrix_v& initialize(const std::string& _fname) {
    clear();
    std::vector< std::vector< T > > another_a;
    try { read_dense< T >(_fname,ORIENT,matrix_base_t::m_size,another_a); }
    catch (const std::runtime_error& e) {
      throw std::runtime_error("dense_matrix_v: " + std::string(e.what()) + " cannot read file.");
    }
    a.resize(size(0)*size(1));
    for (size_t i=0, k=0; i<(ORIENT? size(0):size(1)); ++i, k+=(ORIENT? size(1):size(0)))
      std::copy(another_a[i].begin(),another_a[i].end(),a.begin()+k);
    return *this;
  }

  // assignments

  dense_matrix_v& operator=(const dense_matrix_v& _other) {
    a = _other.a;
    matrix_base_t::m_size = _other.dense_matrix_v::matrix_base_t::m_size;
    return *this;
  }

  dense_matrix_v& operator=(const double& _value) {
    return initialize(size(0),size(1),_value);
  }

  void swap(dense_matrix_v& other) {
    other.a.swap(a);
    matrix_base_t::swap(other);
  }

  // clearing

  dense_matrix_v& clear() {
    a.clear();
    matrix_base_t::m_size.clear();
    return *this;
  }

  dense_matrix_v& zerorow(const size_t& i) {
    if (i>=size(0))
      throw std::runtime_error("dense_matrix_v: row index outside bounds.");
    if (ORIENT) std::fill_n(a.begin()+i*size(1),size(1),T());
    else {
      for (size_t j=0, k=i; j<size(1); ++j, k+=size(0))
        a[k] = T();
    }
    return *this;
  }

  // indexing
  const T& operator()(const size_t& i, const size_t& j=0) const { return ORIENT? a[i*matrix_base_t::m_size.j+j]:a[j*matrix_base_t::m_size.i+i]; }
        T& operator()(const size_t& i, const size_t& j=0)       { return ORIENT? a[i*matrix_base_t::m_size.j+j]:a[j*matrix_base_t::m_size.i+i]; }

  // storage
  std::vector< T > a;
};


/**
 * @brief Sparse matrix: compressed sparse row (3-array variant, most common)
 * T: storage type
 * BASE: column & row numbering base (0 or 1, other bases probably won't work)
 */
template< typename T, int BASE=0, typename ROWSORT=vector_sorted_with_diagonal_first_t >
struct sparse_matrix_csr :
    matrix< T,sparse_matrix_csr< T, BASE > >
{
  typedef matrix< T,sparse_matrix_csr< T, BASE > > matrix_base_t;
  typedef index_compressed_sparse_row_t< BASE > matrix_index_t;
  using matrix_base_t::size;

  // cons/destructor
  sparse_matrix_csr() : matrix_base_t() { clear(); }
  ~sparse_matrix_csr() { clear(); }

  // initializations

  sparse_matrix_csr& initialize(const size_t& i, const size_t& j, const double& _value=double()) {
    if (idx_t(i,j)==matrix_base_t::m_size)
      return operator=(_value);
    throw std::runtime_error("sparse_matrix_csr: resizing not available.");
    return *this;
  }

  sparse_matrix_csr& initialize(const std::vector< double >& _vector) {
    throw std::runtime_error("sparse_matrix_csr: initialize from vector is not possible.");
    return *this;
  }

  sparse_matrix_csr& initialize(const std::string& _fname) {
    clear();
    try { read_sparse< T >(_fname,true,BASE,matrix_base_t::m_size,a,idx.ia,idx.ja); }
    catch (const std::runtime_error& e) {
      throw std::runtime_error("sparse_matrix_csr: " + std::string(e.what()) + " cannot read file.");
    }
    idx.nnu = static_cast< int >(idx.ia.size())-1;
    idx.nnz = static_cast< int >(idx.ja.size());
    if (idx.ja.size()!=a.size()
     || size(0)!=idx.nnu
     || size(1)<=*std::max_element(idx.ja.begin(),idx.ja.end())-BASE)
      throw std::runtime_error("sparse_matrix_csr: after reading file, indexing not correct.");
    return *this;
  }

  sparse_matrix_csr& clear() {
    matrix_base_t::m_size.clear();
    idx.clear();
    a.clear();
    return *this;
  }

  sparse_matrix_csr& operator=(const double& _value) {
    a.assign(a.size(),static_cast< const T >(_value));
    return *this;
  }

  sparse_matrix_csr& operator=(const sparse_matrix_csr& _other) {
    clear();
    matrix_base_t::m_size = _other.matrix_base_t::m_size;
    idx = _other.idx;
    a = _other.a;
    return *this;
  }

  sparse_matrix_csr& zerorow(const size_t& _i) {
    for (int i=idx.ia[_i]-BASE; i<idx.ia[_i+1]-BASE; ++i)
      a[i] = T();
    return *this;
  }

  sparse_matrix_csr& swap(sparse_matrix_csr& other) {
    other.a.swap(a);
    other.idx.swap(idx);
    matrix_base_t::swap(other);
    return *this;
  }

  std::ostream& print(std::ostream& o) const {

    const T eps = 1.e3*std::numeric_limits< T >::epsilon();
    const print_t print_level(matrix_base_t::m_print? matrix_base_t::m_print :
                             (matrix_base_t::m_size.i>100 || matrix_base_t::m_size.j>100? print_size  :
                             (matrix_base_t::m_size.i> 10 || matrix_base_t::m_size.j> 10? print_signs :
                                                                                          print_full )));

    o << "(" << size(0) << 'x' << size(1) << ">=" << a.size() << ") [ ";
    if      (print_level==print_size)  { o << "..."; }
    else if (print_level==print_signs) {
      std::string str;
      for (size_t i=0; i<size(0); ++i) {
        str.assign(size(1),'.');
        for (int k=idx.ia[i]-BASE; k<idx.ia[i+1]-BASE; ++k)
          str[ idx.ja[k]-BASE ] = (a[k]>eps? '+' : (a[k]<-eps? '-' : '0' ));
        o << "\n  " << str;
      }
    }
    else if (print_level==print_full) {
      for (size_t i=0; i<size(0); ++i) {
        std::vector< T > row(size(1),T());
        for (int k=idx.ia[i]-BASE; k<idx.ia[i+1]-BASE; ++k)
          row[ idx.ja[k]-BASE ] = a[k];
        o << "\n  ";
        std::copy(row.begin(),row.end(),std::ostream_iterator< T >(o,", "));
      }
    }
    return o << " ]";
  }

  // indexing
  const T& operator()(const size_t& _i, const size_t& _j) const {
    idx_t ij(_i,_j);
    if (idx.dereference(ij)<matrix_base_t::m_size)
      return a[ij.i];
    throw std::runtime_error("sparse_matrix_csr: index not available.");
    return matrix_base_t::m_zero;
  }

  T& operator()(const size_t& _i, const size_t& _j) {
    idx_t ij(_i,_j);
    if (idx.dereference(ij)<matrix_base_t::m_size)
      return a[ij.i];
    throw std::runtime_error("sparse_matrix_csr: index not available.");
    return matrix_base_t::m_zero;
  }

  // storage
  std::vector< T > a;
  matrix_index_t idx;
};


}  // namespace detail
}  // namespace lss
}  // namespace cf3


#endif
