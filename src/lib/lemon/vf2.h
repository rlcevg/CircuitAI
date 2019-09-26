/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2015
 * EMAXA Kutato-fejleszto Kft. (EMAXA Research Ltd.)
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

#ifndef LEMON_VF2_H
#define LEMON_VF2_H

///\ingroup graph_properties
///\file
///\brief VF2 algorithm \cite cordella2004sub.

#include <lemon/core.h>
#include <lemon/concepts/graph.h>
#include <lemon/dfs.h>
#include <lemon/bfs.h>

#include <vector>
#include <set>

namespace lemon
{
  namespace bits
  {
    namespace vf2
    {
      class AlwaysEq
      {
      public:
        template<class T1, class T2>
        bool operator()(T1, T2) const
        {
          return true;
        }
      };

      template<class M1, class M2>
      class MapEq
      {
        const M1 &_m1;
        const M2 &_m2;
      public:
        MapEq(const M1 &m1, const M2 &m2) : _m1(m1), _m2(m2) {}
        bool operator()(typename M1::Key k1, typename M2::Key k2) const
        {
          return _m1[k1] == _m2[k2];
        }
      };

      template <class G>
      class DfsLeaveOrder : public DfsVisitor<G>
      {
        const G &_g;
        std::vector<typename G::Node> &_order;
        int i;
      public:
        DfsLeaveOrder(const G &g, std::vector<typename G::Node> &order)
          : i(countNodes(g)), _g(g), _order(order)
        {}
        void leave(const typename G::Node &node)
        {
          _order[--i]=node;
        }
      };

      template <class G>
      class BfsLeaveOrder : public BfsVisitor<G>
      {
        int i;
        const G &_g;
        std::vector<typename G::Node> &_order;
      public:
        BfsLeaveOrder(const G &g, std::vector<typename G::Node> &order)
          : i(0), _g(g), _order(order)
        {}
        void process(const typename G::Node &node)
        {
          _order[i++]=node;
        }
      };
    }
  }

  ///Graph mapping types.

  ///\ingroup graph_isomorphism
  ///The \ref Vf2 "VF2" algorithm is capable of finding different kind of
  ///embeddings, this enum specifies its type.
  ///
  ///See \ref graph_isomorphism for a more detailed description.
  enum Vf2MappingType {
    /// Subgraph isomorphism
    SUBGRAPH = 0,
    /// Induced subgraph isomorphism
    INDUCED = 1,
    /// Graph isomorphism

    /// If the two graph has the same number of nodes, than it is
    /// equivalent to \ref INDUCED, and if they also have the same
    /// number of edges, then it is also equivalent to \ref SUBGRAPH.
    ///
    /// However, using this setting is faster than the other two
    /// options.
    ISOMORPH = 2
  };

  ///%VF2 algorithm class.

  ///\ingroup graph_isomorphism This class provides an efficient
  ///implementation of the %VF2 algorithm \cite cordella2004sub
  ///for variants of the (Sub)graph Isomorphism problem.
  ///
  ///There is also a \ref vf2() "function-type interface" called \ref vf2()
  ///for the %VF2 algorithm, which is probably more convenient in most
  ///use-cases.
  ///
  ///\tparam G1 The type of the graph to be embedded.
  ///The default type is \ref ListDigraph.
  ///\tparam G2 The type of the graph g1 will be embedded into.
  ///The default type is \ref ListDigraph.
  ///\tparam M The type of the NodeMap storing the mapping.
  ///By default, it is G1::NodeMap<G2::Node>
  ///\tparam NEQ A bool-valued binary functor determinining whether a node is
  ///mappable to another. By default it is an always true operator.
  ///
  ///\sa vf2()
#ifdef DOXYGEN
  template<class G1, class G2, class M, class NEQ >
#else
  template<class G1=ListDigraph,
           class G2=ListDigraph,
           class M = typename G1::template NodeMap<G2::Node>,
           class NEQ = bits::vf2::AlwaysEq >
#endif
  class Vf2
  {
    //Current depth in the DFS tree.
    int _depth;
    //Functor with bool operator()(G1::Node,G2::Node), which returns 1
    //if and only if the 2 nodes are equivalent.
    NEQ _nEq;

    typename G2::template NodeMap<int> _conn;
    //Current mapping. We index it by the nodes of g1, and match[v] is
    //a node of g2.
    M &_mapping;
    //order[i] is the node of g1, for which we find a pair if depth=i
    std::vector<typename G1::Node> order;
    //currEdgeIts[i] is an edge iterator, witch is last used in the ith
    //depth to find a pair for order[i].
    std::vector<typename G2::IncEdgeIt> currEdgeIts;
    //The small graph.
    const G1 &_g1;
    //The big graph.
    const G2 &_g2;
    //lookup tables for cut the searchtree
    typename G1::template NodeMap<int> rNew1t,rInOut1t;

    Vf2MappingType _mapping_type;

    //cut the search tree
    template<Vf2MappingType MT>
    bool cut(const typename G1::Node n1,const typename G2::Node n2) const
    {
      int rNew2=0,rInOut2=0;
      for(typename G2::IncEdgeIt e2(_g2,n2); e2!=INVALID; ++e2)
        {
          const typename G2::Node currNode=_g2.oppositeNode(n2,e2);
          if(_conn[currNode]>0)
            ++rInOut2;
          else if(MT!=SUBGRAPH&&_conn[currNode]==0)
            ++rNew2;
        }
      switch(MT)
        {
        case INDUCED:
          return rInOut1t[n1]<=rInOut2&&rNew1t[n1]<=rNew2;
        case SUBGRAPH:
          return rInOut1t[n1]<=rInOut2;
        case ISOMORPH:
          return rInOut1t[n1]==rInOut2&&rNew1t[n1]==rNew2;
        default:
          return false;
        }
    }

    template<Vf2MappingType MT>
    bool feas(const typename G1::Node n1,const typename G2::Node n2)
    {
      if(!_nEq(n1,n2))
        return 0;

      for(typename G1::IncEdgeIt e1(_g1,n1); e1!=INVALID; ++e1)
        {
          const typename G1::Node currNode=_g1.oppositeNode(n1,e1);
          if(_mapping[currNode]!=INVALID)
            --_conn[_mapping[currNode]];
        }
      bool isIso=1;
      for(typename G2::IncEdgeIt e2(_g2,n2); e2!=INVALID; ++e2)
        {
          const typename G2::Node currNode=_g2.oppositeNode(n2,e2);
          if(_conn[currNode]<-1)
            ++_conn[currNode];
          else if(MT!=SUBGRAPH&&_conn[currNode]==-1)
            {
              isIso=0;
              break;
            }
        }

      for(typename G1::IncEdgeIt e1(_g1,n1); e1!=INVALID; ++e1)
        {
          const typename G1::Node currNode=_g1.oppositeNode(n1,e1);
          if(_mapping[currNode]!=INVALID&&_conn[_mapping[currNode]]!=-1)
            {
              switch(MT)
                {
                case INDUCED:
                case ISOMORPH:
                  isIso=0;
                  break;
                case SUBGRAPH:
                  if(_conn[_mapping[currNode]]<-1)
                    isIso=0;
                  break;
                }
              _conn[_mapping[currNode]]=-1;
            }
        }
      return isIso&&cut<MT>(n1,n2);
    }

    void addPair(const typename G1::Node n1,const typename G2::Node n2)
    {
      _conn[n2]=-1;
      _mapping.set(n1,n2);
      for(typename G2::IncEdgeIt e2(_g2,n2); e2!=INVALID; ++e2)
        if(_conn[_g2.oppositeNode(n2,e2)]!=-1)
          ++_conn[_g2.oppositeNode(n2,e2)];
    }

    void subPair(const typename G1::Node n1,const typename G2::Node n2)
    {
      _conn[n2]=0;
      _mapping.set(n1,INVALID);
      for(typename G2::IncEdgeIt e2(_g2,n2); e2!=INVALID; ++e2)
        {
          const typename G2::Node currNode=_g2.oppositeNode(n2,e2);
          if(_conn[currNode]>0)
            --_conn[currNode];
          else if(_conn[currNode]==-1)
            ++_conn[n2];
        }
    }

    void setOrder()//we will find pairs for the nodes of g1 in this order
    {
      // bits::vf2::DfsLeaveOrder<G1> v(_g1,order);
      //   DfsVisit<G1,bits::vf2::DfsLeaveOrder<G1> >dfs(_g1, v);
      //   dfs.run();

      //it is more efficient in practice than DFS
      bits::vf2::BfsLeaveOrder<G1> v(_g1,order);
      BfsVisit<G1,bits::vf2::BfsLeaveOrder<G1> >bfs(_g1, v);
      bfs.run();
    }

    template<Vf2MappingType MT>
    bool extMatch()
    {
      while(_depth>=0)
        {
          //there are not nodes in g1, which has not pair in g2.
          if(_depth==static_cast<int>(order.size()))
            {
              --_depth;
              return true;
            }
          //the node of g2, which neighbours are the candidates for
          //the pair of order[_depth]
          typename G2::Node currPNode;
          if(currEdgeIts[_depth]==INVALID)
            {
              typename G1::IncEdgeIt fstMatchedE(_g1,order[_depth]);
              //if _mapping[order[_depth]]!=INVALID, we dont use
              //fstMatchedE
              if(_mapping[order[_depth]]==INVALID)
                for(; fstMatchedE!=INVALID &&
                      _mapping[_g1.oppositeNode(order[_depth],
                                              fstMatchedE)]==INVALID;
                    ++fstMatchedE) ; //find fstMatchedE
              if(fstMatchedE==INVALID||_mapping[order[_depth]]!=INVALID)
                {
                  //We did not find an covered neighbour, this means
                  //the graph is not connected(or _depth==0).  Every
                  //uncovered(and there are some other properties due
                  //to the spec. problem types) node of g2 is
                  //candidate.  We can read the iterator of the last
                  //tryed node from the match if it is not the first
                  //try(match[order[_depth]]!=INVALID)
                  typename G2::NodeIt n2(_g2);
                  //if its not the first try
                  if(_mapping[order[_depth]]!=INVALID)
                    {
                      n2=++typename G2::NodeIt(_g2,_mapping[order[_depth]]);
                      subPair(order[_depth],_mapping[order[_depth]]);
                    }
                  for(; n2!=INVALID; ++n2)
                    if(MT!=SUBGRAPH&&_conn[n2]==0)
                      {
                        if(feas<MT>(order[_depth],n2))
                          break;
                      }
                    else if(MT==SUBGRAPH&&_conn[n2]>=0)
                      if(feas<MT>(order[_depth],n2))
                        break;
                  // n2 is the next candidate
                  if(n2!=INVALID)
                    {
                      addPair(order[_depth],n2);
                      ++_depth;
                    }
                  else // there is no more candidate
                    --_depth;
                  continue;
                }
              else
                {
                  currPNode=_mapping[_g1.oppositeNode(order[_depth],
                                                      fstMatchedE)];
                  currEdgeIts[_depth]=typename G2::IncEdgeIt(_g2,currPNode);
                }
            }
          else
            {
              currPNode=_g2.oppositeNode(_mapping[order[_depth]],
                                         currEdgeIts[_depth]);
              subPair(order[_depth],_mapping[order[_depth]]);
              ++currEdgeIts[_depth];
            }
          for(; currEdgeIts[_depth]!=INVALID; ++currEdgeIts[_depth])
            {
              const typename G2::Node currNode =
                _g2.oppositeNode(currPNode, currEdgeIts[_depth]);
              //if currNode is uncovered
              if(_conn[currNode]>0&&feas<MT>(order[_depth],currNode))
                {
                  addPair(order[_depth],currNode);
                  break;
                }
            }
          currEdgeIts[_depth]==INVALID?--_depth:++_depth;
        }
      return false;
    }

    //calc. the lookup table for cut the searchtree
    void setRNew1tRInOut1t()
    {
      typename G1::template NodeMap<int> tmp(_g1,0);
      for(unsigned int i=0; i<order.size(); ++i)
        {
          tmp[order[i]]=-1;
          for(typename G1::IncEdgeIt e1(_g1,order[i]); e1!=INVALID; ++e1)
            {
              const typename G1::Node currNode=_g1.oppositeNode(order[i],e1);
              if(tmp[currNode]>0)
                ++rInOut1t[order[i]];
              else if(tmp[currNode]==0)
                ++rNew1t[order[i]];
            }
          for(typename G1::IncEdgeIt e1(_g1,order[i]); e1!=INVALID; ++e1)
            {
              const typename G1::Node currNode=_g1.oppositeNode(order[i],e1);
              if(tmp[currNode]!=-1)
                ++tmp[currNode];
            }
        }
    }
  public:
    ///Constructor

    ///Constructor

    ///\param g1 The graph to be embedded into \e g2.
    ///\param g2 The graph \e g1 will be embedded into.
    ///\param m \ref concepts::ReadWriteMap "read-write" NodeMap
    ///storing the found mapping.
    ///\param neq A bool-valued binary functor determinining whether a node is
    ///mappable to another. By default it is an always true operator.
    Vf2(const G1 &g1, const G2 &g2,M &m, const NEQ &neq = NEQ() ) :
      _nEq(neq), _conn(g2,0), _mapping(m), order(countNodes(g1)),
      currEdgeIts(countNodes(g1),INVALID), _g1(g1), _g2(g2), rNew1t(g1,0),
      rInOut1t(g1,0), _mapping_type(SUBGRAPH)
    {
      _depth=0;
      setOrder();
      setRNew1tRInOut1t();
      for(typename G1::NodeIt n(g1);n!=INVALID;++n)
        m[n]=INVALID;
    }

    ///Returns the current mapping type

    ///Returns the current mapping type
    ///
    Vf2MappingType mappingType() const { return _mapping_type; }
    ///Sets mapping type

    ///Sets mapping type.
    ///
    ///The mapping type is set to \ref SUBGRAPH by default.
    ///
    ///\sa See \ref Vf2MappingType for the possible values.
    void mappingType(Vf2MappingType m_type) { _mapping_type = m_type; }

    ///Finds a mapping

    ///It finds a mapping between from g1 into g2 according to the mapping
    ///type set by \ref mappingType(Vf2MappingType) "mappingType()".
    ///
    ///By subsequent calls, it returns all possible mappings one-by-one.
    ///
    ///\retval true if a mapping is found.
    ///\retval false if there is no (more) mapping.
    bool find()
    {
      switch(_mapping_type)
        {
        case SUBGRAPH:
          return extMatch<SUBGRAPH>();
        case INDUCED:
          return extMatch<INDUCED>();
        case ISOMORPH:
          return extMatch<ISOMORPH>();
        default:
          return false;
        }
    }
  };

  template<class G1, class G2>
  class Vf2WizardBase
  {
  protected:
    typedef G1 Graph1;
    typedef G2 Graph2;

    const G1 &_g1;
    const G2 &_g2;

    Vf2MappingType _mapping_type;

    typedef typename G1::template NodeMap<typename G2::Node> Mapping;
    bool _local_mapping;
    void *_mapping;
    void createMapping()
    {
      _mapping = new Mapping(_g1);
    }

    typedef bits::vf2::AlwaysEq NodeEq;
    NodeEq _node_eq;

    Vf2WizardBase(const G1 &g1,const G2 &g2)
      : _g1(g1), _g2(g2), _mapping_type(SUBGRAPH), _local_mapping(true) {}
  };

  /// Auxiliary class for the function-type interface of %VF2 algorithm.

  /// This auxiliary class implements the named parameters of
  /// \ref vf2() "function-type interface" of \ref Vf2 algorithm.
  ///
  /// \warning This class should only be used through the function \ref vf2().
  ///
  /// \tparam TR The traits class that defines various types used by the
  /// algorithm.
  template<class TR>
  class Vf2Wizard : public TR
  {
    typedef TR Base;
    typedef typename TR::Graph1 Graph1;
    typedef typename TR::Graph2 Graph2;

    typedef typename TR::Mapping Mapping;
    typedef typename TR::NodeEq NodeEq;

    using TR::_g1;
    using TR::_g2;
    using TR::_mapping_type;
    using TR::_mapping;
    using TR::_node_eq;

  public:
    ///Constructor
    Vf2Wizard(const Graph1 &g1,const Graph2 &g2) : Base(g1,g2) {}

    ///Copy constructor
    Vf2Wizard(const Base &b) : Base(b) {}


    template<class T>
    struct SetMappingBase : public Base {
      typedef T Mapping;
      SetMappingBase(const Base &b) : Base(b) {}
    };

    ///\brief \ref named-templ-param "Named parameter" for setting
    ///the mapping.
    ///
    ///\ref named-templ-param "Named parameter" function for setting
    ///the map that stores the found embedding.
    template<class T>
    Vf2Wizard< SetMappingBase<T> > mapping(const T &t)
    {
      Base::_mapping=reinterpret_cast<void*>(const_cast<T*>(&t));
      Base::_local_mapping = false;
      return Vf2Wizard<SetMappingBase<T> >(*this);
    }

    template<class NE>
    struct SetNodeEqBase : public Base {
      typedef NE NodeEq;
      NodeEq _node_eq;
      SetNodeEqBase(const Base &b, const NE &node_eq)
        : Base(b), _node_eq(node_eq) {}
    };

    ///\brief \ref named-templ-param "Named parameter" for setting
    ///the node equivalence relation.
    ///
    ///\ref named-templ-param "Named parameter" function for setting
    ///the equivalence relation between the nodes.
    ///
    ///\param node_eq A bool-valued binary functor determinining
    ///whether a node is mappable to another. By default it is an
    ///always true operator.
    template<class T>
    Vf2Wizard< SetNodeEqBase<T> > nodeEq(const T &node_eq)
    {
      return Vf2Wizard<SetNodeEqBase<T> >(SetNodeEqBase<T>(*this,node_eq));
    }

    ///\brief \ref named-templ-param "Named parameter" for setting
    ///the node labels.
    ///
    ///\ref named-templ-param "Named parameter" function for setting
    ///the node labels defining equivalence relation between them.
    ///
    ///\param m1 It is arbitrary \ref concepts::ReadMap "readable node map"
    ///of g1.
    ///\param m2 It is arbitrary \ref concepts::ReadMap "readable node map"
    ///of g2.
    ///
    ///The value type of these maps must be equal comparable.
    template<class M1, class M2>
    Vf2Wizard< SetNodeEqBase<bits::vf2::MapEq<M1,M2> > >
    nodeLabels(const M1 &m1,const M2 &m2)
    {
      return nodeEq(bits::vf2::MapEq<M1,M2>(m1,m2));
    }

    ///\brief \ref named-templ-param "Named parameter" for setting
    ///the mapping type.
    ///
    ///\ref named-templ-param "Named parameter" for setting
    ///the mapping type.
    ///
    ///The mapping type is set to \ref SUBGRAPH by default.
    ///
    ///\sa See \ref Vf2MappingType for the possible values.
    Vf2Wizard<Base> &mappingType(Vf2MappingType m_type)
    {
      _mapping_type = m_type;
      return *this;
    }

    ///\brief \ref named-templ-param "Named parameter" for setting
    ///the mapping type to \ref INDUCED.
    ///
    ///\ref named-templ-param "Named parameter" for setting
    ///the mapping type to \ref INDUCED.
    Vf2Wizard<Base> &induced()
    {
      _mapping_type = INDUCED;
      return *this;
    }

    ///\brief \ref named-templ-param "Named parameter" for setting
    ///the mapping type to \ref ISOMORPH.
    ///
    ///\ref named-templ-param "Named parameter" for setting
    ///the mapping type to \ref ISOMORPH.
    Vf2Wizard<Base> &iso()
    {
      _mapping_type = ISOMORPH;
      return *this;
    }

    ///Runs VF2 algorithm.

    ///This method runs VF2 algorithm.
    ///
    ///\retval true if a mapping is found.
    ///\retval false if there is no (more) mapping.
    bool run()
    {
      if(Base::_local_mapping)
        Base::createMapping();

      Vf2<Graph1, Graph2, Mapping, NodeEq >
        alg(_g1, _g2, *reinterpret_cast<Mapping*>(_mapping), _node_eq);

      alg.mappingType(_mapping_type);

      bool ret = alg.find();

      if(Base::_local_mapping)
        delete reinterpret_cast<Mapping*>(_mapping);

      return ret;
    }
  };

  ///Function-type interface for VF2 algorithm.

  /// \ingroup graph_isomorphism
  ///Function-type interface for VF2 algorithm \cite cordella2004sub.
  ///
  ///This function has several \ref named-func-param "named parameters"
  ///declared as the members of class \ref Vf2Wizard.
  ///The following examples show how to use these parameters.
  ///\code
  ///  // Find an embedding of graph g into graph h
  ///  ListGraph::NodeMap<ListGraph::Node> m(g);
  ///  vf2(g,h).mapping(m).run();
  ///
  ///  // Check whether graphs g and h are isomorphic
  ///  bool is_iso = vf2(g,h).iso().run();
  ///\endcode
  ///\warning Don't forget to put the \ref Vf2Wizard::run() "run()"
  ///to the end of the expression.
  ///\sa Vf2Wizard
  ///\sa Vf2
  template<class G1, class G2>
  Vf2Wizard<Vf2WizardBase<G1,G2> > vf2(const G1 &g1, const G2 &g2)
  {
    return Vf2Wizard<Vf2WizardBase<G1,G2> >(g1,g2);
  }

}

#endif
