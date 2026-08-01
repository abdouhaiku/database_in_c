/* btree.c
 * B-tree implementation in C, following the conventions of Lafore ch. 10:
 *   - ORDER = max number of children per node (5 here, like Figure 10.25;
 *     raise it to hundreds for a real disk-based tree)
 *   - a node holds at most ORDER-1 keys, and children = keys + 1
 *   - insertion goes into the leaf FIRST, then splits propagate bottom-up
 *   - a split distributes keys evenly and promotes the MIDDLE key of the
 *     sequence formed by (existing keys + new key)
 *
 * Build:  gcc -Wall -Wextra -o btree btree.c
 * Run:    ./btree
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define ORDER 5                     /* max children per node            */
#define MAX_KEYS (ORDER - 1)        /* max legal keys per node          */
#define MIN_KEYS ((ORDER - 1) / 2)  /* min keys for non-root nodes      */

/* ------------------------------------------------------------------ */
/* Node                                                                */
/* ------------------------------------------------------------------ */
/* Arrays are ONE SLOT LARGER than the legal maximum. This lets us
 * insert into a full node (temporarily holding ORDER keys), and only
 * then split it — the bottom-up scheme the book describes for 2-3
 * trees and B-trees. The overflow state never survives past split(). */
typedef struct BTreeNode {
    int  numKeys;                          /* keys currently in node    */
    long keys[MAX_KEYS + 1];               /* +1 overflow slot          */
    struct BTreeNode *children[ORDER + 1]; /* +1 overflow slot          */
    struct BTreeNode *parent;
    bool isLeaf;
} BTreeNode;

typedef struct {
    BTreeNode *root;
} BTree;

/* ------------------------------------------------------------------ */
/* Construction / destruction                                          */
/* ------------------------------------------------------------------ */
static BTreeNode *node_create(bool isLeaf)
{
    BTreeNode *n = malloc(sizeof *n);
    if (!n) { perror("malloc"); exit(EXIT_FAILURE); }
    n->numKeys = 0;
    n->parent  = NULL;
    n->isLeaf  = isLeaf;
    for (int i = 0; i <= ORDER; i++)
        n->children[i] = NULL;
    return n;
}

BTree *btree_create(void)
{
    BTree *t = malloc(sizeof *t);
    if (!t) { perror("malloc"); exit(EXIT_FAILURE); }
    t->root = node_create(true);           /* empty tree = empty leaf   */
    return t;
}

static void node_destroy(BTreeNode *n)
{
    if (!n) return;
    if (!n->isLeaf)
        for (int i = 0; i <= n->numKeys; i++)
            node_destroy(n->children[i]);
    free(n);
}

void btree_destroy(BTree *t)
{
    node_destroy(t->root);
    free(t);
}

/* ------------------------------------------------------------------ */
/* Search                                                              */
/* ------------------------------------------------------------------ */
/* Same walk as the 2-3-4 tree's find(): linear scan inside the node
 * (fine for small ORDER; use binary search for large nodes), then
 * descend into the child whose key range brackets the target.        */
bool btree_search(const BTree *t, long key)
{
    const BTreeNode *cur = t->root;
    while (cur) {
        int i = 0;
        while (i < cur->numKeys && key > cur->keys[i])
            i++;                            /* skip smaller keys        */
        if (i < cur->numKeys && cur->keys[i] == key)
            return true;                    /* found in this node       */
        if (cur->isLeaf)
            return false;                   /* nowhere left to descend  */
        cur = cur->children[i];             /* child i holds the range  */
    }
    return false;
}

bool btree_search_rec(BTreeNode *t, long key) {
    if (t == NULL) return false;

    int i = t->numKeys - 1;              /* start at the LAST real key */
    while (i >= 0 && t->keys[i] > key)
        i--;

    if (i >= 0 && t->keys[i] == key)      /* guard with i>=0, not i<numKeys */
        return true;

    return btree_search_rec(t->children[i + 1], key);
}

/* ------------------------------------------------------------------ */
/* Insertion helpers                                                   */
/* ------------------------------------------------------------------ */
/* Insert key (and, for internal nodes, the child that goes to its
 * right) into a node at the correct sorted position. The node may
 * end up holding MAX_KEYS+1 keys — the caller must then split it.    */
static void node_insert_at(BTreeNode *n, long key, BTreeNode *rightChild)
{
    int i = n->numKeys - 1;
    while (i >= 0 && n->keys[i] > key) {   /* shift bigger keys right  */
        n->keys[i + 1] = n->keys[i];
        n->children[i + 2] = n->children[i + 1];
        i--;
    }
    n->keys[i + 1] = key;
    n->children[i + 2] = rightChild;
    if (rightChild)
        rightChild->parent = n;
    n->numKeys++;
}

/* Split an overflowing node (numKeys == MAX_KEYS + 1 == ORDER).
 * Middle key is promoted to the parent; left half stays, right half
 * moves to a new sibling. Recurses upward if the parent overflows —
 * the "ripple" of Figure 10.20, applied to B-trees.                  */
static void split(BTree *t, BTreeNode *n)
{
    int mid = n->numKeys / 2;              /* ORDER=5 -> mid index 2   */
    long promoted = n->keys[mid];

    /* --- build the new right sibling ------------------------------ */
    BTreeNode *right = node_create(n->isLeaf);
    right->numKeys = n->numKeys - mid - 1;
    for (int i = 0; i < right->numKeys; i++)
        right->keys[i] = n->keys[mid + 1 + i];
    if (!n->isLeaf) {
        for (int i = 0; i <= right->numKeys; i++) {
            right->children[i] = n->children[mid + 1 + i];
            if (right->children[i])
                right->children[i]->parent = right;
            n->children[mid + 1 + i] = NULL;
        }
    }
    n->numKeys = mid;                      /* left half stays put      */

    /* --- promote the middle key ----------------------------------- */
    if (n->parent == NULL) {
        /* Splitting the root: the ONLY way the tree gets taller.     */
        BTreeNode *newRoot = node_create(false);
        newRoot->keys[0] = promoted;
        newRoot->numKeys = 1;
        newRoot->children[0] = n;
        newRoot->children[1] = right;
        n->parent = right->parent = newRoot;
        t->root = newRoot;
    } else {
        node_insert_at(n->parent, promoted, right);
        if (n->parent->numKeys > MAX_KEYS) /* parent overflowed too?   */
            split(t, n->parent);           /* ripple upward            */
    }
}

/* ------------------------------------------------------------------ */
/* Insertion                                                           */
/* ------------------------------------------------------------------ */
void btree_insert(BTree *t, long key)
{
    /* Phase 1: descend to the correct leaf (no splitting on the way
     * down — that's the 2-3-4 top-down style; B-trees split only when
     * an actual overflow happens).                                    */
    BTreeNode *cur = t->root;
    while (!cur->isLeaf) {
        int i = 0;
        while (i < cur->numKeys && key > cur->keys[i]) {
            if (cur->keys[i] == key) return;   /* no duplicates        */
            i++;
        }
        if (i < cur->numKeys && cur->keys[i] == key)
            return;                            /* no duplicates        */
        cur = cur->children[i];
    }
    for (int i = 0; i < cur->numKeys; i++)
        if (cur->keys[i] == key)
            return;                            /* no duplicates        */

    /* Phase 2: insert into the leaf, then split if it overflowed.     */
    node_insert_at(cur, key, NULL);
    if (cur->numKeys > MAX_KEYS)
        split(t, cur);
}

/* ------------------------------------------------------------------ */
/* Display (depth-first, level/child format like Listing 10.1)         */
/* ------------------------------------------------------------------ */
static void rec_display(const BTreeNode *n, int level, int childNum)
{
    printf("level=%d child=%d ", level, childNum);
    for (int i = 0; i < n->numKeys; i++)
        printf("/%ld", n->keys[i]);
    printf("/\n");
    if (!n->isLeaf)
        for (int i = 0; i <= n->numKeys; i++)
            if (n->children[i])
                rec_display(n->children[i], level + 1, i);
}

void btree_display(const BTree *t)
{
    rec_display(t->root, 0, 0);
}


static void stage(const char *label, const BTree *t)
{
    printf("---- stage %s ----\n", label);
    btree_display(t);
    printf("\n");
}

int main(void)
{
    BTree *t = btree_create();

    /* a) root fills up: [20|40|60|80], 70 about to be inserted */
    btree_insert(t, 20);
    btree_insert(t, 40);
    btree_insert(t, 60);
    btree_insert(t, 80);
    stage("a  (root full, before inserting 70)", t);

    /* b) 70 overflows the root: seq 20,40,60,70,80 -> 60 promoted,
     *    new root created. Then 10 and 30 are inserted.             */
    btree_insert(t, 70);
    stage("b  (after root split on 70)", t);

    /* c) 10 and 30 fill the left leaf; 15 about to be inserted */
    btree_insert(t, 10);
    btree_insert(t, 30);
    stage("c  (left leaf full, before inserting 15)", t);

    /* d) 15 splits the left leaf: seq 10,15,20,30,40 -> 20 promoted */
    btree_insert(t, 15);
    stage("d  (after leaf split on 15)", t);

    /* e) 75, 85 fill the right leaf; 90 splits it:
     *    seq 70,75,80,85,90 -> 80 promoted                          */
    btree_insert(t, 75);
    btree_insert(t, 85);
    btree_insert(t, 90);
    stage("e  (after leaf split on 90)", t);

    /* f) 25, 35 fill the second leaf; 50 splits it:
     *    seq 25,30,35,40,50 -> 35 promoted. Root is now full.       */
    btree_insert(t, 25);
    btree_insert(t, 35);
    btree_insert(t, 50);
    stage("f  (after leaf split on 50; root now full)", t);

    /* g) 22 and 27 go into [25|30] WITHOUT splits, even though the
     *    root above is full — splits only fire on real overflow.    */
    btree_insert(t, 22);
    btree_insert(t, 27);
    stage("g  (22, 27 inserted with NO splits)", t);

    /* h)+i) 32 overflows [22|25|27|30]: seq 22,25,27,30,32 -> 27 up;
     *    but the root [20|35|60|80] is full too, so it also splits:
     *    seq 20,27,35,60,80 -> 35 up -> new root, height grows.     */
    btree_insert(t, 32);
    stage("i  (after double split on 32 — final tree)", t);

    /* sanity: every inserted key must be found */
    long all[] = {10,15,20,22,25,27,30,32,35,40,50,60,70,75,80,85,90};
    int n = (int)(sizeof all / sizeof all[0]), ok = 1;
    for (int i = 0; i < n; i++)
        if (!btree_search(t, all[i])) { ok = 0;
            printf("MISSING KEY: %ld\n", all[i]); }
    printf("search check: %s\n", ok ? "all 17 keys found" : "FAILED");

    btree_destroy(t);
    return 0;
}
