#ifndef IG_H
#define IG_H


#include "IGCommon.h"

#include <algorithm>


class IG {
    igraph_t graph;

public:
    IG() { empty(); }

    ~IG() {
        igraph_destroy(&graph);
    }

    // Helpers

    void igConstructorCheck(int err) {
        if (! err) return;
        empty(); // make sure 'graph' is not left uninitialized
        throw mma::LibraryError(igraph_strerror(err));
    }

    // Create (basic)

    void empty() { igraph_empty(&graph, 0, false); } // does not free the graph, should not be exposed to Mathematica

    void fromEdgeList(mma::RealTensorRef v, mint n, bool directed) {
        igraph_destroy(&graph);
        igraph_vector_t edgelist = igVectorView(v);
        igConstructorCheck(igraph_create(&graph, &edgelist, n, directed));
    }

    // Create (games)

    void degreeSequenceGame(mma::RealTensorRef outdeg, mma::RealTensorRef indeg, mint method) {
        igraph_vector_t ig_indeg = igVectorView(indeg);
        igraph_vector_t ig_outdeg = igVectorView(outdeg);
        igraph_degseq_t ig_method;
        switch (method) {
        case 0: ig_method = IGRAPH_DEGSEQ_SIMPLE; break;
        case 1: ig_method = IGRAPH_DEGSEQ_SIMPLE_NO_MULTIPLE; break;
        case 2: ig_method = IGRAPH_DEGSEQ_VL; break;
        default: throw mma::LibraryError("degreeSequenceGame: unknown method option.");
        }

        igraph_destroy(&graph);
        int err;
        if (indeg.length() == 0)
            err = igraph_degree_sequence_game(&graph, &ig_outdeg, NULL, ig_method);
        else
            err = igraph_degree_sequence_game(&graph, &ig_outdeg, &ig_indeg, ig_method);
        igConstructorCheck(err);
    }

    // Structure

    mma::RealTensorRef edgeList() const {
        igVector vec;
        igCheck(igraph_get_edgelist(&graph, &vec.vec, false));
        mma::RealTensorRef res = mma::makeMatrix<double>(vec.length() / 2, 2);
        std::copy(vec.begin(), vec.end(), res.begin());
        return res;
    }

    mint edgeCount() const { return igraph_ecount(&graph); }

    mint vertexCount() const { return igraph_vcount(&graph); }

    // Testing

    bool directedQ() const { return igraph_is_directed(&graph); }

    bool dagQ() const {
        igraph_bool_t res;
        igCheck(igraph_is_dag(&graph, &res));
        return res;
    }

    bool simpleQ() const {
        igraph_bool_t res;
        igCheck(igraph_is_simple(&graph, &res));
        return res;
    }

    // TODO handle strong/weak for directed
    bool connectedQ() const {
        igraph_bool_t res;
        igCheck(igraph_is_connected(&graph, &res, IGRAPH_STRONG));
        return res;
    }

    // Centrality measures

    mma::RealTensorRef betweenness() const {
        igVector vec;
        igCheck(igraph_betweenness(&graph, &vec.vec, igraph_vss_all(), true, NULL, true));
        return vec.makeMTensor();
    }

    mma::RealTensorRef closeness(bool normalized) const {
        igVector vec;
        igCheck(igraph_closeness(&graph, &vec.vec, igraph_vss_all(), IGRAPH_OUT, NULL, normalized));
        return vec.makeMTensor();
    }

    mma::RealTensorRef edgeBetweenness() const {
        igVector vec;
        igCheck(igraph_edge_betweenness(&graph, &vec.vec, true, NULL));
        return vec.makeMTensor();
    }

    // Randomize

    void rewire(mint n, bool loops) {
        if (n > std::numeric_limits<igraph_integer_t>::max())
            throw mma::LibraryError("igraph rewire: Requested number of rewiring trials too large.");
        igCheck(igraph_rewire(&graph, n, loops ? IGRAPH_REWIRING_SIMPLE_LOOPS : IGRAPH_REWIRING_SIMPLE));
    }

    void rewireEdges(double prob, bool loops, bool multiple) {
        igCheck(igraph_rewire_edges(&graph, prob, loops, multiple));
    }

    // Isomorphism

    bool isomorphic(IG &ig) const {
        igraph_bool_t res;
        igCheck(igraph_isomorphic(&graph, &ig.graph, &res));
        return res;
    }

    bool subisomorphic(IG &ig) const {
        igraph_bool_t res;
        igCheck(igraph_subisomorphic(&graph, &ig.graph, &res));
        return res;
    }

    mint isoclass() const {
        igraph_integer_t res;
        igCheck(igraph_isoclass(&graph, &res));
        return res;
    }

    // Topological sorting, directed acylic graphs

    // see also dagQ() under Testing

    mma::RealTensorRef topologicalSorting() const {
        igVector vec;
        igCheck(igraph_topological_sorting(&graph, &vec.vec, IGRAPH_OUT));
        return vec.makeMTensor();
    }

    mma::RealTensorRef feedbackArcSet(bool exact) const {
        igVector vec;
        igCheck(igraph_feedback_arc_set(&graph, &vec.vec, NULL, exact ? IGRAPH_FAS_EXACT_IP : IGRAPH_FAS_APPROX_EADES));
        return vec.makeMTensor();
    }

    // Motifs and subgraph counts

    mma::IntTensorRef dyadCensus() const {
        igraph_integer_t mut, asym, none;
        igCheck(igraph_dyad_census(&graph, &mut, &asym, &none));
        mma::IntTensorRef res = mma::makeVector<mint>(3);
        res[0] = mut;
        res[1] = asym;
        res[2] = none;
        return res;
    }

    mma::RealTensorRef triadCensus() const {
        igVector vec;
        igCheck(igraph_triad_census(&graph, &vec.vec));
        return vec.makeMTensor();
    }

    mma::RealTensorRef motifs(mint size, mma::RealTensorRef cut_prob) const {
        igVector vec;
        igraph_vector_t ig_cut_prob = igVectorView(cut_prob);
        igCheck(igraph_motifs_randesu(&graph, &vec.vec, size, &ig_cut_prob));
        return vec.makeMTensor();
    }

    mint motifsNo(mint size, mma::RealTensorRef cut_prob) const {
        igraph_integer_t res;
        igraph_vector_t ig_cut_prob = igVectorView(cut_prob);
        igCheck(igraph_motifs_randesu_no(&graph, &res, size, &ig_cut_prob));
        return res;
    }

    mint motifsEstimate(mint size, mma::RealTensorRef cut_prob, mint sample_size) const {
        igraph_integer_t res;
        igraph_vector_t ig_cut_prob = igVectorView(cut_prob);
        igCheck(igraph_motifs_randesu_estimate(&graph, &res, size, &ig_cut_prob, sample_size, NULL));
        return res;
    }

    // Shortest paths

    mma::RealMatrixRef shortestPaths() const {
        igMatrix res;
        igCheck(igraph_shortest_paths(&graph, &res.mat, igraph_vss_all(), igraph_vss_all(), IGRAPH_OUT));
        return res.makeMTensor();
    }
};

#endif // IG_H

