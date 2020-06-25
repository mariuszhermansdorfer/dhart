/// \file graph.h \brief Header file for a graph ADT
///
/// \author TBA \date 06 Jun 2020 \todo Forward declares for eigen.

#pragma once

#include <robin_hood.h>
#include <vector>
#include <Edge.h>
#include <Node.h>
#include <Eigen>

namespace Eigen {
}

/// <summary> Output CSR for interop with other languages </summary>
namespace HF {
	namespace SpatialStructures {


		/*! \brief Methods of aggregating the costs for edges for each node in the graph. 
		
		\see Graph.AggregateGraph() for details on how to use this enum.
		*/
		enum class COST_AGGREGATE : int {
			/// Add the cost of all edges.
			SUM = 0,
			/// Average the cost of all edges
			AVERAGE = 1,
			/// Count how many edges this node has.
			COUNT = 2
		};

		/*! \brief A struct to hold all necessary information for a CSR.

			\remarks 
			This can be used by external clients to recreate or map to 
			an existing CSR. For more information on what these arrays contain
			see
			https://eigen.tuxfamily.org/dox/group__TutorialSparse.html
		*/
		struct CSRPtrs {
			int nnz;	///< Number of non-zeros contained by the CSR
			int rows;	///< Number of rows in this CSR.
			int cols;	///< Number of columns in this CSR.

			float* data;			///< Pointer to the CSR's data array.
			int* outer_indices;		///< Pointer to the CSR's outer indices array. 
			int* inner_indices;		///< Pointer to the CSR's inner indices array.

			/// <summary> Verify the CSR referenced by this instance is valid. </summary>
			/// <returns>
			/// True if data, outer_indices, and inner_indices are non-null, false otherwise,
			/// </returns>
			/*!
				\details
				Simply checks that all the contained arrays are not null. 

				\remarks
				For now, this just checks if any of the arrays are null since Eigen arrays will return
				null pointers if they are in an invalid state (uninitialized, uncompressed, etc). 

				\code
					// be sure to #include "graph.h", and #include <memory>
			
					std::unique_ptr<float[]> data(new float[16]);
					std::unique_ptr<int[]> outer_indices(new int[16]);
					std::unique_ptr<int[]> inner_indices(new int[16]);

					float* p_data = data.get();
					int* p_outer_indices = outer_indices.get();
					int* p_inner_indices = inner_indices.get();

					HF::SpatialStructures::CSRPtrs csr = { 16, 16, 16, p_data, p_outer_indices, p_inner_indices };

					bool validity = csr.AreValid();	// validity == true, since all pointer fields are non-null
				\endcode
			*/
			inline bool AreValid() {
				return (data && outer_indices && inner_indices);
			}
		};

		/*! \brief A Graph of nodes connected by edges that supports both integers and HF::SpatialStructures::Node.
			
			\details
			Internally, this object uses Eigen (https://eigen.tuxfamily.org/dox/group__TutorialSparse.html)
			to store and maintain a CSR matrix.

			\invariant
				Every node in the graph will have a Unique ID with no repeats

		*/
		class Graph {
		private:
			std::vector<Node> ordered_nodes;				///< A list of nodes contained by the graph.
			std::vector<int> id_to_nodes;					///< Maps ids to indexes in ordered_nodes.
			robin_hood::unordered_map<Node, int> idmap;		///< Maps a list of X,Y,Z positions to ids
			Eigen::SparseMatrix<float, 1> edge_matrix;		///< The underlying CSR containing edge nformation.
			int next_id = 0;								///< The id for the next unique node.
			std::vector<Eigen::Triplet<float>> triplets;	///< Edges to be converted to a CSR when Graph::Compress() is called.
			bool needs_compression = true;					///< If true, the CSR is inaccurate and requires compression.

			/// <summary>
			/// Get the unique ID for this x, y, z position, assigning it an new one if it doesn't
			/// already exist
			/// </summary>
			/*!
				\details 
				If the node has not yet been seen by the graph, next_id will be assigned to it and incremented,
				then the node node will and its new id will be added to idmap. If the node has already been 
				assigned an ID, then the ID will be returned directly from idmap. 

				\param input_node Node to retrieve and potentially assign a new ID for. 
				\returns The ID of input_node. 

				\code
					// definition of Graph::addEdge(const Node& parent, const Node& child, float score)
				\endcode
			*/
			/// \snippet spatialstructures\src\graph.cpp GetOrAssignID_Node
			int getOrAssignID(const Node& input_node);

			/*!
				\brief Add an ID to the graph if it doesn't exist already.

				\code
					// definition of Graph::addEdge(int parent_id, int child_id, float score)
				\endcode
			*/
			/// \snippet spatialstructures\src\graph.cpp GetOrAssignID_int
			int getOrAssignID(int input_int);


			/*!
				\brief Determine if an edge between parent and child exists in the graph.
				
				\details 
				Iterates through every row in the parent's column to find child. If child
				is not found in this column, false is returned. If child can be found in this
				column, then true is returned.

				\par Time Complexity
				O(k) where k is the number of edges from parent.
				
			*/
			/// \snippet spatialstructures\src\graph.cpp CheckForEdge
			bool checkForEdge(int parent, int child) const;

			/// <summary> Add an empty node for this new id </summary>
			/*! \deprecated Never Implemented.*/
			void UpdateIDs(int new_id);

		public:
			/*!
			 \brief Construct a graph from a list of nodes, edges, and distances. 

			 \param edges Ordered array of arrays of edges for each node in nodes.
			 \param distances Ordered array of distance from parent to child for each edge in edges.
			 \param Nodes Ordered array of nodes to act as a parent to all children in it's array in edges.

			\details Preallocates the matrix it in element by element and compresses it. 

			 \pre 1) The size of all input arrays must match: 
			 `(edges.size() == nodes.size() && nodes.size() == distances.size())`
			
			\pre 2) For the node at `nodes[i]`, `edges[i]` should contain an array for the id of all nodes
			that `nodes[i]` has an edge from and, and `distances[i]` should contain an array of the the distance
			from `nodes[i]` to one of the nodes it has an edge to in `edges[i]`.

			\note After constructing a graph with this constructor, it will not be able to be modified.
			Use the empty constructor and use addEdge if you want to modify the graph after construction.
			This may change in the future. 

			 \remarks
			 This constructor can offer "slightly higher performance and memory consumption"
			 than constructing a graph using Graph::addEdge in a loop according to official eigen documentation,
			 however it may not be feasible for certain situations where the entire graph isn't
			 known before the constructor is called. 
			 The implementation is based on the algorithm from Eigen's documentation under
			 the section Filling a Sparse Matrix 
			 https://eigen.tuxfamily.org/dox/group__TutorialSparse.html.

			\code
				// be sure to #include "graph.h"
		
				// Create the nodes
				HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
				HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
				HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
		
				// Create a container (vector) of nodes
				std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
		
				// Create matrices for edges and distances, edges.size() == distances().size()
				std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
				std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
		
				// Now you can create a Graph - note that nodes, edges, and distances are passed by reference
				HF::SpatialStructures::Graph graph(edges, distances, nodes);
			\endcode
			*/
			Graph(
				const std::vector<std::vector<int>> & edges, 
				const std::vector<std::vector<float>> & distances, 
				const std::vector<Node> & Nodes
			);

			/*! \brief Construct an empty graph.

				\remarks This can be used to create a new graph to later be filled with edges/nodes
				by calling Graph::addEdge() then calling Graph::Compress(). Implementation
				is based on the Eigen documentation for Filling a CSR:
				https://eigen.tuxfamily.org/dox/group__TutorialSparse.html.

				
				\see Graph::addEdge() for details on adding edges.
				\see Graph::Compress() for details on compressing the graph.

				\code
					// be sure to #include "graph.h"
					HF::SpatialStructures::Graph graph;		// This represents an order-zero graph (null graph)
															// It lacks vertices and edges.
				\endcode
			*/
			Graph() {};

			/*! \brief Determine if the graph has an edge from parent to child.
			
				\param parent Parent of the edge to check for.
				\param child Child of the edge to check for.
				\param undirected If true, look for an edge from child to parent as well.
				\returns True if an edge between parent and child exists 
				(also child and parent if undirected is true).

				\remarks
				Converts parent and child to Node then calls the node overload.
				
				\exception std::exception if the matrix is uncompressed.
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					HF::SpatialStructures::Graph graph(edges, distances, nodes);									  
			
					// Prepare {x, y, z} coordinates (positions)
					auto parent_pos = node_1.getArray();		// (2.0, 3.0, 4.0)
					auto child_pos = node_2.getArray();			// (11.0, 22.0, 140.0)
			
					// last argument can be true/false for undirected/directed graph respectively
					bool has_edge = graph.HasEdge(parent_pos, child_pos, true);
				\endcode
			*/
			bool HasEdge(
				const std::array<float, 3>& parent,
				const std::array<float, 3>& child,
				bool undirected = false
			) const;

			/*! \brief Determine if the graph has an edge from parent to child.
			
				\param parent Parent of the edge to check for.
				\param child Child of the edge to check for.
				\param undirected If true, look for an edge from child to parent as well.
				\returns True if an edge between parentand child exists
				(also child and parent if undirected is true).

				\remarks
				Gets the IDs of both nodes, then calls the integer overload.
				
				\exception std::exception if the graph is uncompressed.
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					// last argument can be true/false for undirected/directed graph respectively
					bool has_edge = graph.HasEdge(node_1, node_2, true);
				\endcode
			*/
			bool HasEdge(const Node& parent, const Node& child, const bool undirected = false) const;
			
			/* \brief Determine if the graph has an edge from parent to child.

				\param parent Parent of the edge to check for.
				\param child Child of the edge to check for.
				\param undirected If true, look for an edge from child to parent as well.
				\returns True if an edge between parentand child exists
				(also child and parent if undirected is true).

				\exception std::exception if the graph is uncompressed.
				
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					// last argument can be true/false for undirected/directed graph respectively
					bool has_edge = graph.HasEdge(0, 1, true);
				\endcode
			*/
			bool HasEdge(int parent, int child, bool undirected = false) const;

			/// <summary> Get a list of nodes from the graph sorted by ID. </summary>
			/// <returns> A sorted vector of nodes. </returns>

			/*!
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					HF::SpatialStructures::Graph graph(edges, distances, nodes);	
			
					// Nodes() returns a copy of the ordered_nodes field
					std::vector<HF::SpatialStructures::Node> nodes_from_graph = graph.Nodes();
				\endcode
			*/
			std::vector<Node> Nodes() const;

			/// <summary> Get a list of all edges to and from node N. </summary>
			/// <param name="N"> The Node to get edges from and to. </param>
			/// <returns>
			/// A list of edges to and from node N or an empty array if is not in the graph.
			/// </returns>
			/*!
				\par Time Complexity
				`O(k)` where k is the number of edges in the graph since it needs to iterate through 
				every edge in the graph to find the edges to this node.

				\see operator[] to get a list of directed edges only containing edges from N.
				
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					HF::SpatialStructures::Graph graph(edges, distances, nodes);	
			
					// Retrieve the nodes from the graph, or use the original instance of
					// std::vector<HF::SpatialStructures::Node> passed to Graph upon instantiation
					std::vector<HF::SpatialStructures::Node> get_nodes = graph.Nodes();
			
					// nodes[index] yields an instance of Node that we can pass to
					// GetUndirectedEdges. Any node that exists with graph can be passed to this
					// member function to retrieve a vector of undirected edges.
					int index = 2;
					std::vector<HF::SpatialStructures::Edge> undirected_edges = graph.GetUndirectedEdges(get_nodes[index]);
				\endcode
			*/
			std::vector<Edge> GetUndirectedEdges(const Node & N) const;
		
			/// <summary> Get every in the given graph as IDs. </summary>
			/// <returns>
			/// An array of edgesets for every node in the graph (Graph in the form of IDs).
			/// </returns>
			/*!
				
				\exception std::exception if the graph hasn't been compressed. compressed.
				
				\par Time Complexity
				O(k) where k is the number of edges in the graph.

				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);	
			
					// graph must be compressed, or a exception will be thrown
			
					// To brief, an EdgeSet has the following layout: struct EdgeSet { int parent;
					// std::vector<IntEdge> children; };
					//
					// An IntEdge has the following layout: struct IntEdge { int child; float
					// weight; };
			
					// A std::vector<EdgeSet> is a Graph, in the form of IDs.
					std::vector<HF::SpatialStructures::EdgeSet> edge_set = graph.GetEdges();
				\endcode
			*/
			std::vector<EdgeSet> GetEdges() const;

			/// <summary>
			/// Summarize the costs of every outgoing edge for every node in the graph.
			/// </summary>
			/// <param name="agg_type"> Type of aggregation to use. </param>
			/// <param name="directed">
			/// If true, include both incoming and outgoing edges for calculating a node's score.
			/// </param>
			/// <returns> An ordered list of scores for agg_type on each node in the graph. </returns>
			/*!
				
				\remarks Useful for getting scores from the VisibilityGraph.

				\exception std::out_of_range if agg_type doesn't match any value of COST_AGGREGATE.
				\exception Std::exception if the graph isn't compressed.

				\par Time Complexity
				If undirected: `O(k)` where k is the total number of edges in the graph.\n
				If directed: `O(n)` where n is the total number of nodes in the graph.

				\see COST_AGGREGATE to see a list of supported aggregation types.
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					// graph must be compressed, or a exception will be thrown
					HF::SpatialStructures::COST_AGGREGATE aggregate = HF::SpatialStructures::COST_AGGREGATE::AVERAGE;		// aggregate == 1 in this case
			 
					// directed parameter may be true or false
					std::vector<float> aggregate_graph = graph.AggregateGraph(aggregate, true);	
				\endcode
			*/
			std::vector<float> AggregateGraph(COST_AGGREGATE agg_type, bool directed=true) const;

			/// <summary> Get a list of all edges from Node n. </summary>
			/// <param name="n"> Parent of all edges to retrieve. </param>
			/// <returns> An array of edges from node n. </returns>
			/// <exception cref="std::outofrange"> n does not exist in the graph. </exception>

			/*!
				\todo Should this just return an empty list instead of throwing?
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					// Retrieve the nodes from the graph, or use the original instance of
					// std::vector<Node> passed to Graph upon instantiation
					std::vector<HF::SpatialStructures::Node> get_nodes = graph.Nodes();
			
					// nodes[index] yields an instance of Node that we can pass to
					// GetUndirectedEdges. Any node that exists with graph can be passed to this
					// member function to retrieve a vector of edges.
					int index = 2;
					HF::SpatialStructures::Node node = get_nodes[index];
			
					// Note that if node does not exist within graph, that an exception will be thrown.
					std::vector<HF::SpatialStructures::Edge> undirected_edges = graph[node];
			
					// See a (node)->(child_node_0, child_node_1, ... child_node_n)
					std::cout << node.getArray() << "->";
					for (auto e : undirected_edges) {
						std::cout << e.child.getArray() << ", ";
					}
					std::cout << std::endl;
				\endcode
			*/
			const std::vector<Edge> operator[](const Node& n) const;

			/// <summary> Add a new edge to the graph from parent to child. </summary>
			/// <param name="parent"> Parent node of the edge. </param>
			/// <param name="child"> Child node of the edge. </param>
			/// <param name="score"> Cost of traversing from aprent to child. </param>
			/*!
				\details 
				If the parent or child node do not have an ID. An ID will be assigned
				automatically.

				\warning
				This will not work if the graph wasn't created from the empty constructor since
				it has no internal edge list to add to.

				\remarks
				This adds a new element to the triplet list so next time Compress
				is called, the value is added to the graph. 

				\todo How should this signal that the graph can't have edges added to it? Or how do
				we add edges to an existing graph quickly without adding to its edge list?

				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					// Create a pair of nodes
					HF::SpatialStructures::Node n_parent(4.0f, 5.0f, 6.0f);
					HF::SpatialStructures::Node n_child(7.0f, 8.0f, 9.0f);
			
					graph.addEdge(n_parent, n_child);	// default score is 1.0f
				\endcode
			*/
			void addEdge(const Node& parent, const Node& child, float score = 1.0f);

			/// <summary> Add a new edge to the graph from parent to child. </summary>
			/// <param name="parent"> Parent node of the edge. </param>
			/// <param name="child"> Child node of the edge. </param>
			/// <param name="score"> Cost of traversing from aprent to child. </param>
			/*!
				\details
				If the parent or child ids don't exist in the dictionary, they will be added.

				\warning
				This will not work if the graph wasn't created from the empty constructor since
				it has no internal edge list to add to.

				\remarks
				This adds a new element to the triplet list so next time Compress is called, 
				the value is added to the graph.

				\todo How should this signal that the graph can't have edges added to it? Or how do
				we add edges to an existing graph quickly without adding to its edge list?
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f, 4);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f, 6);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					int parent = 1;
					int child = 2;
			
					graph.addEdge(parent, child, 1.0f);
					graph.Compress();
				\endcode
			*/
			void addEdge(int parent_id, int child_id, float score);

			/// <summary> Determine if n exists in the graph. </summary>
			/// <param name="n"> Node to look for. </param>
			/// <returns> True if the node exists, false otherwise. </returns>
			
			/*!

				\details
				Performs a single hash to check if n exists in the hashmap.

				\par Time Complexity
				O(1) since it's a single hash function.

				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f, 4);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f, 6);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					HF::SpatialStructures::Node other_node(55.0f, 66.1f, 15.5f, 9510);	// Let's construct a Node we know is not in graph.
					bool has_key = graph.hasKey(other_node);	// other_node does not exist in graph, so has_key == false;
			
					// Likewise, if we pass a Node instance that indeed exists...
			
					// Retrieve the nodes from the graph, or use the original instance of
					// std::vector<Node> passed to Graph upon instantiation
					std::vector<HF::SpatialStructures::Node> get_nodes = graph.Nodes();
			
					// nodes[index] yields an instance of Node that we can pass to hasKey. Any node
					// that exists with graph can be passed to this member function to determine if
					// the graph has the node's key, or not.
					int index = 2;
					HF::SpatialStructures::Node good_node = get_nodes[index];
			
					has_key = graph.hasKey(good_node);		// now has_key is true
				\endcode
			*/
			bool hasKey(const Node& n) const;

			/// <summary> Get a list of nodes as float arrays. </summary>
			/// <returns>
			/// An array of float arrays containing the position of every node in the graph in order.
			/// </returns>
			/*!
				\remarks
				May be useful for to functions that take arrays instead of nodes.
				
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f, 4);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f, 6);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					// A container of std::array<float, 3> is constructed and populated within
					// NodesAsFloat3, and returned. Each array of 3 floats represents a Node's
					// position within the Cartesian coordinate system. { x, y, z }
					std::vector<std::array<float, 3>> nodes_as_floats = graph.NodesAsFloat3();
			
					// The two loops below will yield the same output
					for (auto n : graph.Nodes()) {
						std::cout << "(" << n.x << "," << n.y << "," << n.z << ")" << std::endl;
					}
			
					for (auto a : nodes_as_floats) {
						std::cout << a << std::endl;
					}
				\endcode
			*/
			std::vector<std::array<float, 3>> NodesAsFloat3() const;

			/// <summary> Determine how many nodes are in the graph. </summary>
			/// <returns> The number of nodes in the graph. </returns>
			/*!
				\details Size is directly returned from `id_to_nodes.size()`. 
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f, 4);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f, 6);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					int id_count = graph.size();		  // We retrieve the size of the node id count within graph (3)
				\endcode
			*/
			int size() const;

			/// <summary> Retrieve the ID for node in this graph. </summary>
			/// <returns> The ID assigned to this node. -1 if it was not yet added to the graph </returns>
			/*!
				\code
					// be sure to #include "graph.h"
			
					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f, 4);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f, 6);
			
					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };
			
					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };
			
					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					HF::SpatialStructures::Node other_node(55.0f, 66.1f, 15.5f, 9510);	// Let's construct a Node we know is not in graph.
					bool has_key = graph.hasKey(other_node);							// other_node does not exist in graph, so has_key == false;
			
					int ID = graph.getID(other_node);									// ID will assigned -1, because other_node is not a part of graph.
			
					// Likewise, if we pass a Node instance that indeed exists...
			
					// Retrieve the nodes from the graph, or use the original instance of
					// std::vector<Node> passed to Graph upon instantiation
					std::vector<HF::SpatialStructures::Node> get_nodes = graph.Nodes();
			
					// nodes[index] yields an instance of Node that we can pass to hasKey. Any node
					// that exists with graph can be passed to this member function to determine if
					// the graph has the node's key, or not.
					int index = 2;					// we assume for this example that index 2 is valid.
					HF::SpatialStructures::Node good_node = get_nodes[index];
			
					ID = graph.getID(good_node);	// ID > -1, i.e. it is a Node instance that exists within this Graph.
				\endcode
			*/
			int getID(const Node& node) const;

			/*!
				\brief Compress the graph to a CSR and enable the usage of several functions.

				\details 
				This won't do anything if called on an already compressed graph. The graph is "compressed"
				by resizing the edge matrix to the maximum ID of any node in triplets, then calling
				setFromTriplets(). 

				\note 
				This function actually doesn't actually reduce memory usage since it keeps the edge list
				in order to allow for modifications to the graph. In the future, it may be beneficial allow for
				the user to pass in a boolean that would delete the triplet array if true. 

				\remarks
				This method of constructing the CSR is based on Eigen's documentation for 
				Filling a sparse matrix https://eigen.tuxfamily.org/dox/group__TutorialSparse.html.

				\code
					// be sure to #include "graph.h"

					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f, 4);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f, 6);

					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };

					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };

					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					// Create a pair of nodes
					HF::SpatialStructures::Node n_parent(4.0f, 5.0f, 6.0f);
					HF::SpatialStructures::Node n_child(7.0f, 8.0f, 9.0f);

					graph.addEdge(n_parent, n_child);	// default score is 1.0f
			
					// In order to use GetEdges, or AggregateGraph, we must compress our graph instance
					graph.Compress();						// GetEdges and AggregateGraph are now usable
				\endcode
			*/			
			void Compress();

			/// <summary>
			/// Obtain the size of and pointers to the 3 arrays that comprise this graph's CSR.
			/// graph if it isn't compressed already
			/// </summary>
			/// <returns> Pointers and sizes of the arrays that comprise the CSR. If the CSR cannot
			/// be constructed due to factors such as an empty input array, then the CSRPtrs contain null
			/// values for it's pointers. </returns>
			
			/*!
				\details This will automatically call Compress if it hasn't been called already.

				\remarks 
				This can be useful for reconstructing or mapping to the CSR without interacting
				with eigen at all. Numpy can directly make map the arrays returned by this function
				to it's own CSR implementation.

				\code
					// be sure to #include "graph.h"

					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f, 4);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f, 6);

					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };

					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };

					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);

					// Create a pair of nodes
					HF::SpatialStructures::Node n_parent(4.0f, 5.0f, 6.0f);
					HF::SpatialStructures::Node n_child(7.0f, 8.0f, 9.0f);

					graph.addEdge(n_parent, n_child);	// default score is 1.0f

					// Graph will be compressed automatically be GetCSRPointers
					CSRPtrs returned_csr = graph.GetCSRPointers();
				\endcode

				\see CSRPtrs.AreValid() for checking if the return value represents a valid CSR.
			*/
			CSRPtrs GetCSRPointers();

			/// <summary> Retrieve the node that corresponds to id. </summary>
			/// <param name="id"> The ID of the node to get. </param>
			/// <returns> The node corresponding to id, by value. </returns>
			/*!
				\exception std::out_of_range id didn't belong to any node in the graph. 
				\code
					// be sure to #include "graph.h"

					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f, 4);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f, 6);

					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };

					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };

					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					// Let's retrieve node_1.
					int desired_node_id = 2;
					HF::SpatialStructures::Node node_from_id = graph.NodeFromID(desired_node_id);
			
					// Note that NodeFromID ceases to work if the id argument provided does not
					// exist as an ID among the nodes within graph
				\endcode
			*/
			Node NodeFromID(int id) const;

			/// <summary> Clear all nodes and edges from the graph. </summary>
			/*!
				\code
					// be sure to #include "graph.h"

					// Create the nodes
					HF::SpatialStructures::Node node_0(1.0f, 1.0f, 2.0f, 4);
					HF::SpatialStructures::Node node_1(2.0f, 3.0f, 4.0f, 5);
					HF::SpatialStructures::Node node_2(11.0f, 22.0f, 140.0f, 6);

					// Create a container (vector) of nodes
					std::vector<HF::SpatialStructures::Node> nodes = { node_0, node_1, node_2 };

					// Create matrices for edges and distances, edges.size() == distances().size()
					std::vector<std::vector<int>> edges = { { 1, 2 }, { 2 }, { 1 } };
					std::vector<std::vector<float>> distances = { { 1.0f, 2.5f }, { 54.0f }, { 39.0f } };

					// Now you can create a Graph - note that nodes, edges, and distances are passed
					// by reference
					// Note: graph is compressed upon instantiation
					HF::SpatialStructures::Graph graph(edges, distances, nodes);
			
					// If we want to remove all nodes and edges from graph, we may do so with Clear:
					graph.Clear();						// edge_matrix is zeroed out, buffer is squeezed,
														// triplets are also cleared, and
														// needs_compression == true
				\endcode
			*/
			void Clear();

			// TODO: Should these even be in the graph?
			/// <summary> Calculate cross slope for the given graph </summary>
			/*!
				\deprecated Not Implemented.
				\code
					// TODO example - code commented out in graph.cpp
				\endcode
			*/
			void GenerateCrossSlope();

			/// <summary> Calculate energy for the given graph </summary>
			/*!
				\deprecated Not Implemented.
				\code
					// TODO example - code commented out in graph.cpp
				\endcode
			*/
			void GenerateEnergy();
		};
	}
}
