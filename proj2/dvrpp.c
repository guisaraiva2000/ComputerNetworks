/***************************************************************************\
* Distance vector routing protocol with reverse path poisoning.             *
\***************************************************************************/

#include <stdlib.h>
#include <stdio.h>

#include "routing-simulator.h"

#define min(a, b) (int) a < (int) b ? a:b

// Message format to send between nodes.
typedef struct {
    cost_t vector_cost[MAX_NODES];
} message_t;

// State format.
typedef struct {
    cost_t distances[MAX_NODES][MAX_NODES];
    node_t next_hop[MAX_NODES];
} state_t;


// Send messages from current_node to all neighbors
void send_msg(state_t *state, node_t current_node) {
    for (node_t n = get_first_node(); n <= get_last_node(); n++) {
        if (n != current_node && get_link_cost(n) != COST_INFINITY) {
            message_t *nm = (message_t *) malloc(sizeof(message_t));
            for (node_t x = get_first_node(); x <= get_last_node(); x++) {
                if (state->next_hop[x] == n) {
                    nm->vector_cost[x] = COST_INFINITY;
                } else {
                    nm->vector_cost[x] = state->distances[current_node][x];
                }
            }
            send_message(n, nm);
        }
    }
}

// Check if there are new routes w Bellman-Ford
void find_routes(state_t *state, node_t current_node){
    for (node_t adj = get_first_node(); adj <= get_last_node(); adj++) {
        if (adj != current_node) {

            cost_t min_cost = get_link_cost(adj);
            cost_t temp_min = get_link_cost(adj);
            node_t temp_neighbor = adj;

            for (node_t n = get_first_node(); n <= get_last_node(); n++) {
                if (n != adj && n != current_node && get_link_cost(n) != COST_INFINITY) {

                    min_cost = min(get_link_cost(n) + state->distances[n][adj], min_cost);

                    if (temp_min > min_cost) {
                        temp_neighbor = n;
                        temp_min = min_cost;
                    }
                }
            }

            if (min_cost != state->distances[current_node][adj]) {
                set_route(adj, temp_neighbor, min_cost);

                state->distances[current_node][adj] = min_cost;
                state->distances[adj][current_node] = min_cost;
                state->next_hop[adj] = temp_neighbor;

                send_msg(state, current_node);
            }
        }
    }
}

// Notify a node that a neighboring link has changed cost.
void notify_link_change(node_t neighbor, cost_t new_cost) {
    int current_node = get_current_node();
    state_t *state = (state_t *) get_state();

    if(!state) { // initial state
        state = (state_t *) malloc(sizeof(state_t));
        set_state(state);

        for (node_t n = get_first_node(); n <= get_last_node(); n++) {
            for (node_t j = get_first_node(); j <= get_last_node(); j++)
                if (!(j == current_node && n == current_node))
                    state->distances[n][j] = COST_INFINITY;

            state->next_hop[n] = COST_INFINITY;
        }
    }

    find_routes(state, current_node);
}

// Receive a message sent by a neighboring node.
void notify_receive_message(node_t sender, void *message) {
    int current_node = get_current_node();
    state_t *state = (state_t *) get_state();
    message_t *m = (message_t *) message;

    for (node_t j = get_first_node(); j <= get_last_node(); j++)
        if (j != sender)
            state->distances[sender][j] = m->vector_cost[j];

    find_routes(state, current_node);
}
