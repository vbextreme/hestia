#define RBTREE_IMPLEMENTATION
#include <notstd/rbtree.h>
#include <math.h>

__private void rbt_leftrotate(rbtNode_s** root, rbtNode_s* p){
    if( p->right == NULL ) return;
    rbtNode_s* y = p->right;
    if( y->left ){
        p->right = y->left;
        y->left->parent = p;
    }
    else{
        p->right=NULL;
	}

	if( p->parent ){
        y->parent = p->parent;
        if( p == p->parent->left ) p->parent->left = y;
        else p->parent->right = y;
	}
	else{
        *root = y;
		y->parent = NULL;
	}

    y->left = p;
    p->parent = y;
}

__private void rbt_rightrotate(rbtNode_s** root, rbtNode_s* p){
    if( p->left == NULL) return ;

    rbtNode_s* y = p->left;
    if( y->right ){
        p->left = y->right;
        y->right->parent = p;
    }
    else{
        p->left = NULL;
	}

	if( p->parent ){
		y->parent = p->parent;
		if( p == p->parent->left ) p->parent->left = y;
		else p->parent->right = y;
	}
	else{
        *root = y;
		y->parent = NULL;
	}
    
	y->right = p;
    p->parent = y;
}

__private void rbt_insertfix(rbtNode_s** root, rbtNode_s *page){
    rbtNode_s* u;
    if( *root == page){
        page->color = RBT_BLACK;
        return;
    }
	iassert(page->parent);
    while( page->parent && page->parent->color == RBT_RED){
        rbtNode_s* g = page->parent->parent;
		iassert(g);
        if( g->left == page->parent ){
            if( g->right ){
                u = g->right;
				if( u->color == RBT_RED ){
                    page->parent->color = RBT_BLACK;
                    u->color = RBT_BLACK;
                    g->color = RBT_RED;
                    page = g;
                }
				else{
					break;
				}
            }
            else{
                if( page->parent->right == page ){
                    page = page->parent;
                    rbt_leftrotate(root, page);
                }
                page->parent->color = RBT_BLACK;
                g->color = RBT_RED;
                rbt_rightrotate(root, g);
            }
        }
        else{
            if( g->left ){
                u = g->left;
				if( u->color == RBT_RED ){
                    page->parent->color = RBT_BLACK;
                    u->color = RBT_BLACK;
                    g->color = RBT_RED;
                    page = g;
                }
				else{
					break;
				}
            }
            else{
                if( page->parent->left == page ){
                    page = page->parent;
                    rbt_rightrotate(root, page);
                }
                page->parent->color = RBT_BLACK;
                g->color = RBT_RED;
                rbt_leftrotate(root, g);
            }
        }
        (*root)->color = RBT_BLACK;
    }
}

rbtNode_s* rbtNode_ctor(rbtNode_s* n, void* data){
	n->color  = RBT_RAINBOW;
	n->data   = data; 
	n->parent = n->left = n->right = NULL;
	return n;
}

void rbtNode_dtor(void* node){
	rbtNode_s* n = node;
	mem_free(n->data);
}

rbtNode_s* rbtree_insert(rbtree_s* rbt, rbtNode_s* page){
	if( page->color != RBT_RAINBOW ) return page;
	page->color = RBT_RED;

    rbtNode_s* p = rbt->root;
    rbtNode_s* q = NULL;

    if( !rbt->root ){ 
		rbt->root = page;
	}
    else{
        while( p != NULL ){
            q = p;
            if( rbt->cmp(p->data, page->data) < 0 )
                p = p->right;
            else
                p = p->left;
        }
        page->parent = q;
        if( rbt->cmp(q->data, page->data) < 0 )
            q->right = page;
        else
            q->left = page;
    }
    rbt_insertfix(&rbt->root, page);
	++rbt->count;
	return page;
}

__private void rbt_removefix(rbtNode_s** root, rbtNode_s* p){
	iassert(p);
    rbtNode_s* s;
    while( p != *root && p->color == RBT_BLACK ){
        if( p->parent->left == p ){
            s = p->parent->right;
            if( s->color == RBT_RED ){
                s->color = RBT_BLACK;
                p->parent->color = RBT_RED;
                rbt_leftrotate(root, p->parent);
                s = p->parent->right;
            }
            if( s->right->color == RBT_BLACK && s->left->color == RBT_BLACK ){
                s->color = RBT_RED;
                p = p->parent;
            }
            else{
                if( s->right->color == RBT_BLACK ){
                    s->left->color = RBT_BLACK;
                    s->color = RBT_RED;
                    rbt_rightrotate(root, s);
                    s = p->parent->right;
                }
                s->color = p->parent->color;
                p->parent->color = RBT_BLACK;
                s->right->color = RBT_BLACK;
                rbt_leftrotate(root, p->parent);
                p = *root;
            }
        }
        else{
            s = p->parent->left;
            if( s->color == RBT_RED ){
                s->color=RBT_BLACK;
                p->parent->color = RBT_RED;
                rbt_rightrotate(root, p->parent);
                s = p->parent->left;
            }
            if( s->left->color == RBT_BLACK && s->right->color == RBT_BLACK){
                s->color = RBT_RED;
                p = p->parent;
            }
            else{
                if(s->left->color == RBT_BLACK){
                    s->right->color = RBT_BLACK;
                    s->color = RBT_RED;
                    rbt_leftrotate(root, s);
                    s = p->parent->left;
                }
                s->color = p->parent->color;
                p->parent->color = RBT_BLACK;
                s->left->color = RBT_BLACK;
                rbt_rightrotate(root, p->parent);
                p = *root;
            }
        }

        p->color=RBT_BLACK;
        (*root)->color = RBT_BLACK;
    }
}

__private rbtNode_s* rbt_successor(rbtNode_s* p){
    rbtNode_s* y = NULL;
    if( p->left ){
        y = p->left;
        while( y->right ) y = y->right;
    }
    else{
        y = p->right;
        while( y->left ) y = y->left;
    }
    return y;
}

rbtNode_s* rbtree_remove(rbtree_s* rbt, rbtNode_s* p){
	if( p->color == RBT_RAINBOW ) return p;
	if( !rbt->root || !p ) return NULL;
	--rbt->count;
	if( (rbt->root) == p && (rbt->root)->left == NULL && (rbt->root)->right == NULL ){
		p->parent = p->left = p->right = NULL;
		rbt->root = NULL;
		p->color = RBT_RAINBOW;
		return p;
	}

    rbtNode_s* y = NULL;
    rbtNode_s* q = NULL;

	if( p->left == NULL || p->right==NULL ) y = p;
	else y = rbt_successor(p);

	if( y->left ) q = y->left;
	else if( y->right ) q = y->right;

	if( q ) q->parent = y->parent;

	if( !y->parent ) rbt->root = q;
	else{
        if( y == y->parent->left ) y->parent->left = q;
        else y->parent->right = q;
    }

    if( y != p ){
		p->color = y->color;
		if( p->parent ){
			if( p->parent->left == p ) p->parent->left = y;
			else if( p->parent->right == p ) p->parent->right = y;
		}
		else{
			rbt->root = y;
		}
		if( y->parent ){
			if( y->parent->left == y ) y->parent->left = p;
			else if( y->parent->right == y ) y->parent->right = p;
		}
		else{
			rbt->root = p;
		}
		swap(p->parent, y->parent);
		swap(p->left, y->left);
		swap(p->right, y->right);
	}
	if( q && y->color == RBT_BLACK ) rbt_removefix(&rbt->root, q);

	if( p->parent ){
		if( p->parent->left == p ) p->parent->left = NULL;
		else if( p->parent->right == p ) p->parent->right = NULL;
	}
	p->parent = p->left = p->right = NULL;
	p->color = RBT_RAINBOW;
	return p;
}

rbtNode_s* rbtree_find(rbtree_s* rbt, const void* key){
	rbtNode_s* p = rbt->root;
	int cmp;
    while( p && (cmp=rbt->cmp(p->data, key)) ){
        p = cmp < 0 ? p->right : p->left;
    }
	return p;
}

void* rbtree_search(rbtree_s* rbt, const void* key){
	rbtNode_s* p = rbt->root;
	int cmp;
    while( p && (cmp=rbt->cmp(p->data, key)) ){
        p = cmp < 0 ? p->right : p->left;
    }
	return p ? p->data : NULL;
}

rbtNode_s* rbtree_find_best(rbtree_s* rbt, const void* key){
	rbtNode_s* p = rbt->root;
	while( p && rbt->cmp(p->data, key) < 0 ){
		p = p->right;
		while( p && p->left && rbt->cmp(p->left->data, key) >= 0 ) p = p->left;
    }
	return p;
}

void* rbtree_iterate_inorder(rbtreeit_s* it){
	if( !it->count ) return NULL;
	while( it->cur ){
		it->stk = mem_push(it->stk, &it->cur);
		it->cur = it->cur->left;
	}
	if( !mem_pop(it->stk, &it->cur) ) return NULL;
	void* ret = it->cur->data;
	it->cur = it->cur->right;
	--it->count;
	return ret;
}

void* rbtree_iterate_preorder(rbtreeit_s* it){
	if( !it->count ) return NULL;
	if( !it->cur && !mem_pop(it->stk, &it->cur) ) return NULL;
	void* ret = it->cur->data;
	if( it->cur->right ) it->stk = mem_push(it->stk, &it->cur->right);
	it->cur = it->cur->left;
	--it->count;
	return ret;
}

rbtreeit_s* rbtreeit_ctor(rbtreeit_s* it, rbtree_s* t, unsigned count){
	if( !count || count > t->count ) count = t->count;
	unsigned h = 2 * log2(count+1);
	if( h < 2 ) h = 2;
	it->count = count;
	it->stk   = MANY(rbtNode_s*, h);
	it->cur   = t->root;
	return it;
}

void rbtreeit_dtor(void* i){
	rbtreeit_s* it = i;
	mem_free(it->stk);
}

rbtree_s* rbtree_ctor(rbtree_s* t, cmp_f fn){
	t->root  = NULL;
	t->cmp   = fn;
	t->count = 0;
	return t;
}

__private void postorder_free(rbtNode_s* node){
	if( !node ) return;
	postorder_free(node->left);
	postorder_free(node->right);
	mem_free(node);
}

void rbtree_dtor(void* tree){
	rbtree_s* t = tree;
	postorder_free(t->root);
}

__private void postorder_free_cbk(rbtNode_s* node, mcleanup_f clean){
	if( !node ) return;
	postorder_free_cbk(node->left, clean);
	postorder_free_cbk(node->right, clean);
	clean(node);
}

void rbtree_dtor_cbk(rbtree_s* rbt, mcleanup_f clean){
	postorder_free_cbk(rbt->root, clean);
}


/*
void map_rbtree_inorder(rbtNode_t* n, rbtMap_f fn, void* arg){
	if( n == NULL || n->color == RBT_RAINBOW ) return;
	map_rbtree_inorder(n->left, fn, arg);
	if( fn(n->data, arg) ) return;
	map_rbtree_inorder(n->right, fn, arg);
}

void map_rbtree_preorder(rbtNode_t* n, rbtMap_f fn, void* arg){
	if( n == NULL || n->color == RBT_RAINBOW ) return;
	if( fn(n->data, arg) ) return;
	map_rbtree_preorder(n->left, fn, arg);
	map_rbtree_preorder(n->right, fn, arg);
}

void map_rbtree_postorder(rbtNode_t* n, rbtMap_f fn, void* arg){
	if( n == NULL || n->color == RBT_RAINBOW ) return;
	map_rbtree_postorder(n->left, fn, arg);
	map_rbtree_postorder(n->right, fn, arg);
	fn(n->data, arg);
}
*/

