// To build on Linux:
//  make HW=ugc

#if defined(LINUX)
#include "include/compiler.h"
#include <pthread.h>
#include <stdio.h>
#include "user/util.h"
#include "include/xsys.h"
#include "include/ugc.hh"
#else
#endif

static gcptr* gcroot;

struct node : public gcptr
{
  node(gcptr* parent)
  {
    ref = parent->ref(this);
  }

  gcptr_ref* ref;
  node*      left;
  node*      right;
};

static node* root;

static void
build(node* r, int depth, int height)
{
  if (depth == height)
    return;

  r->left = new node(r);
  r->right = new node(r);
  
  build(r->left, depth + 1, height);
  build(r->right, depth + 1, height);
}

int
main(int argc, char** argv)
{
  gcroot = gcptr::new_root();
  root = new node(gcroot);
  build(root, 1, 16);

  gcroot->uref(root->ref);
  gcollect(gcroot);

  return 0;
}
