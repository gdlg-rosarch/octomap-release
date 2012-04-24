
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <octomap/octomap_timing.h>
#include <octomap/octomap.h>
#include <octomap/math/Utils.h>
#include "testing.h"

using namespace std;
using namespace octomap;

void printUsage(char* self){
  std::cerr << "\nUSAGE: " << self << " input.bt\n\n";


  exit(1);
}

void computeChildCenter (const unsigned int& pos,
    const float& center_offset,
    const point3d& parent_center,
    point3d& child_center) {
  // x-axis
  if (pos & 1) child_center(0) = parent_center(0) + center_offset;
  else     child_center(0) = parent_center(0) - center_offset;

  // y-axis
  if (pos & 2) child_center(1) = parent_center(1) + center_offset;
  else   child_center(1) = parent_center(1) - center_offset;
  // z-axis
  if (pos & 4) child_center(2) = parent_center(2) + center_offset;
  else   child_center(2) = parent_center(2) - center_offset;
}

/// mimics old deprecated behavior to compare against
void getLeafNodesRecurs(std::list<OcTreeVolume>& voxels,
    unsigned int max_depth,
    OcTreeNode* node, unsigned int depth,
    const point3d& parent_center, const point3d& tree_center,
    OcTree* tree, bool occupied)
{
  if ((depth <= max_depth) && (node != NULL) ) {
    if (node->hasChildren() && (depth != max_depth)) {

      double center_offset = tree_center(0) / pow( 2., (double) depth+1);
      point3d search_center;

      for (unsigned int i=0; i<8; i++) {
        if (node->childExists(i)) {

          computeChildCenter(i, center_offset, parent_center, search_center);
          getLeafNodesRecurs(voxels, max_depth, node->getChild(i), depth+1, search_center, tree_center, tree, occupied);

        } // GetChild
      }
    }
    else {
      if (tree->isNodeOccupied(node) == occupied){
        double voxelSize = tree->getResolution() * pow(2., double(16 - depth));
        voxels.push_back(std::make_pair(parent_center - tree_center, voxelSize));

      }
    }
  }
}


/// mimics old deprecated behavior to compare against
void getVoxelsRecurs(std::list<OcTreeVolume>& voxels,
                                       unsigned int max_depth,
                                       OcTreeNode* node, unsigned int depth,
                                       const point3d& parent_center, const point3d& tree_center,
                                       double resolution){

  if ((depth <= max_depth) && (node != NULL) ) {
    if (node->hasChildren() && (depth != max_depth)) {

      double center_offset = tree_center(0) / pow(2., (double) depth + 1);
      point3d search_center;

      for (unsigned int i = 0; i < 8; i++) {
        if (node->childExists(i)) {
          computeChildCenter(i, (float) center_offset, parent_center, search_center);
          getVoxelsRecurs(voxels, max_depth, node->getChild(i), depth + 1, search_center, tree_center, resolution);

        }
      } // depth
    }
    double voxelSize = resolution * pow(2., double(16 - depth));
    voxels.push_back(std::make_pair(parent_center - tree_center, voxelSize));
  }
}


/// compare two lists of octree nodes on equality
void compareResults(const std::list<OcTreeVolume>& list_iterator, const std::list<OcTreeVolume>& list_depr){
  EXPECT_EQ(list_iterator.size(), list_depr.size());
  list<OcTreeVolume>::const_iterator list_it = list_iterator.begin();
  list<OcTreeVolume>::const_iterator list_depr_it = list_depr.begin();

  for (; list_it != list_iterator.end(); ++list_it, ++list_depr_it){
    EXPECT_NEAR(list_it->first.x(), list_depr_it->first.x(), 0.005);
    EXPECT_NEAR(list_it->first.y(), list_depr_it->first.y(), 0.005);
    EXPECT_NEAR(list_it->first.z(), list_depr_it->first.z(), 0.005);
  }
  std::cout << "Resulting lists (size "<< list_iterator.size() << ") identical\n";
}

// for unique comparing, need to sort the lists:
bool OcTreeVolumeSortPredicate(const OcTreeVolume& lhs, const OcTreeVolume& rhs)
{
  return ( lhs.second < rhs.second
      ||  (lhs.second == rhs.second &&
          lhs.first.x() < rhs.first.x()
          &&  lhs.first.y() < rhs.first.y()
          &&  lhs.first.z() < rhs.first.z()));
}


double timediff(const timeval& start, const timeval& stop){
  return (stop.tv_sec - start.tv_sec) + 1.0e-6 *(stop.tv_usec - start.tv_usec);
}

int main(int argc, char** argv) {


  //##############################################################     

  string btFilename = "";
  unsigned char maxDepth = 16;


  // test timing:
  timeval start;
  timeval stop;
  double time_it, time_depr;

  if (argc < 1|| argc >3 || strcmp(argv[1], "-h") == 0){
    printUsage(argv[0]);
  }

  btFilename = std::string(argv[1]);
  if (argc > 2){
    maxDepth = (unsigned char)atoi(argv[2]);
  }
  maxDepth = std::min((unsigned char)16,maxDepth);


  cout << "\nReading OcTree file\n===========================\n";
  OcTree* tree = new OcTree(btFilename);
  if (tree->size()<= 1){
    std::cout << "Error reading file, exiting!\n";
    return 1;
  }

  size_t count;
  std::list<OcTreeVolume> list_depr;
  std::list<OcTreeVolume> list_iterator;

  /**
   * get number of nodes:
   */
  gettimeofday(&start, NULL);  // start timer
  size_t num_leafs_recurs = tree->getNumLeafNodes();
  gettimeofday(&stop, NULL);  // stop timer
  time_depr = timediff(start, stop);

  gettimeofday(&start, NULL);  // start timer
  size_t num_leafs_it = 0;
  for(OcTree::leaf_iterator it = tree->begin(), end=tree->end(); it!= end; ++it) {
    num_leafs_it++;
  }
  gettimeofday(&stop, NULL);  // stop timer
  time_it = timediff(start, stop);
  std::cout << "Number of leafs: " << num_leafs_it << " / " << num_leafs_recurs << ", times: "
        <<time_it << " / " << time_depr << "\n========================\n\n";


  /**
   * get all occupied leafs
   */
  const unsigned char tree_depth(16);
  const unsigned int tree_max_val(32768);
  point3d tree_center;
  tree_center(0) = tree_center(1) = tree_center(2)
              = (float) (((double) tree_max_val) * tree->getResolution());

  gettimeofday(&start, NULL);  // start timer
  getLeafNodesRecurs(list_depr,tree_depth,tree->getRoot(), 0, tree_center, tree_center, tree, true);
  gettimeofday(&stop, NULL);  // stop timer
  time_depr = timediff(start, stop);

  gettimeofday(&start, NULL);  // start timer
  for(OcTree::iterator it = tree->begin(), end=tree->end(); it!= end; ++it){
    if(tree->isNodeOccupied(*it))
    {
      //count ++;
     list_iterator.push_back(OcTreeVolume(it.getCoordinate(), it.getSize()));
    }

  }
  gettimeofday(&stop, NULL);  // stop timer
  time_it = timediff(start, stop);

  std::cout << "Occupied lists traversed, times: "
      <<time_it << " / " << time_depr << "\n";
  compareResults(list_iterator, list_depr);
  std::cout << "========================\n\n";


  /**
   * get all free leafs
   */
  list_iterator.clear();
  list_depr.clear();
  gettimeofday(&start, NULL);  // start timer
  for(OcTree::leaf_iterator it = tree->begin(), end=tree->end(); it!= end; ++it) {
    if(!tree->isNodeOccupied(*it))
      list_iterator.push_back(OcTreeVolume(it.getCoordinate(), it.getSize()));
  }
  gettimeofday(&stop, NULL);  // stop timer
  time_it = timediff(start, stop);

  gettimeofday(&start, NULL);  // start timer
  getLeafNodesRecurs(list_depr,tree_depth,tree->getRoot(), 0, tree_center, tree_center, tree, false);
  gettimeofday(&stop, NULL);  // stop timer
  time_depr = timediff(start, stop);

  std::cout << "Free lists traversed, times: "
      <<time_it << " / " << time_depr << "\n";
  compareResults(list_iterator, list_depr);
    std::cout << "========================\n\n";



  /**
   * get all volumes
   */
  list_iterator.clear();
  list_depr.clear();

  gettimeofday(&start, NULL);  // start timer
  getVoxelsRecurs(list_depr,tree_depth,tree->getRoot(), 0, tree_center, tree_center, tree->getResolution());
  gettimeofday(&stop, NULL);  // stop timer
  time_depr = timediff(start, stop);

  gettimeofday(&start, NULL);  // start timers
  for(OcTree::tree_iterator it = tree->begin_tree(), end=tree->end_tree();
      it!= end; ++it){
      //count ++;
     list_iterator.push_back(OcTreeVolume(it.getCoordinate(), it.getSize()));
  }
  gettimeofday(&stop, NULL);  // stop timer
  time_it = timediff(start, stop);

  list_iterator.sort(OcTreeVolumeSortPredicate);
  list_depr.sort(OcTreeVolumeSortPredicate);

  std::cout << "All inner lists traversed, times: "
      <<time_it << " / " << time_depr << "\n";
  compareResults(list_iterator, list_depr);
    std::cout << "========================\n\n";



    // traverse all leaf nodes, timing:
    gettimeofday(&start, NULL);  // start timers
    count = 0;
    for(OcTree::iterator it = tree->begin(maxDepth), end=tree->end();
        it!= end; ++it){
      // do something:
      count++;
    }

    gettimeofday(&stop, NULL);  // stop timer
    time_it = timediff(start, stop);

    std::cout << "Time to traverse all leafs at max depth " <<(unsigned int)maxDepth <<" ("<<count<<" nodes): "<< time_it << " s\n\n";




  /**
   * bounding box tests
   */
  //tree->expand();
  // test complete tree (should be equal to no bbx)
  OcTreeKey bbxMinKey, bbxMaxKey;
  double temp_x,temp_y,temp_z;
  tree->getMetricMin(temp_x,temp_y,temp_z);
  octomap::point3d bbxMin(temp_x,temp_y,temp_z);

  tree->getMetricMax(temp_x,temp_y,temp_z);
  octomap::point3d bbxMax(temp_x,temp_y,temp_z);

  EXPECT_TRUE(tree->genKey(bbxMin, bbxMinKey));
  EXPECT_TRUE(tree->genKey(bbxMax, bbxMaxKey));

  OcTree::leaf_bbx_iterator it_bbx = tree->begin_leafs_bbx(bbxMinKey,bbxMaxKey);
  EXPECT_TRUE(it_bbx == tree->begin_leafs_bbx(bbxMinKey,bbxMaxKey));
  OcTree::leaf_bbx_iterator end_bbx = tree->end_leafs_bbx();
  EXPECT_TRUE(end_bbx == tree->end_leafs_bbx());

  OcTree::leaf_iterator it = tree->begin_leafs();
  EXPECT_TRUE(it == tree->begin_leafs());
  OcTree::leaf_iterator end = tree->end_leafs();
  EXPECT_TRUE(end == tree->end_leafs());


  for( ; it!= end && it_bbx != end_bbx; ++it, ++it_bbx){
    EXPECT_TRUE(it == it_bbx);
  }
  EXPECT_TRUE(it == end && it_bbx == end_bbx);


  // now test an actual bounding box:
  tree->expand(); // (currently only works properly for expanded tree (no multires)
  bbxMin = point3d(-1, -1, - 1);
  bbxMax = point3d(3, 2, 1);
  EXPECT_TRUE(tree->genKey(bbxMin, bbxMinKey));
  EXPECT_TRUE(tree->genKey(bbxMax, bbxMaxKey));

  typedef std::tr1::unordered_map<OcTreeKey, double, OcTreeKey::KeyHash> KeyVolumeMap;

  KeyVolumeMap bbxVoxels;

  count = 0;
  for(OcTree::leaf_bbx_iterator it = tree->begin_leafs_bbx(bbxMinKey,bbxMaxKey), end=tree->end_leafs_bbx();
      it!= end; ++it)
  {
    count++;
    OcTreeKey currentKey = it.getKey();
    // leaf is actually a leaf:
    EXPECT_FALSE(it->hasChildren());

    // leaf exists in tree:
    OcTreeNode* node = tree->search(currentKey);
    EXPECT_TRUE(node);
    EXPECT_EQ(node, &(*it));
    // all leafs are actually in the bbx:
    for (unsigned i = 0; i < 3; ++i){
//      if (!(currentKey[i] >= bbxMinKey[i] && currentKey[i] <= bbxMaxKey[i])){
//        std::cout << "Key failed: " << i << " " << currentKey[i] << " "<< bbxMinKey[i] << " "<< bbxMaxKey[i]
//             << "size: "<< it.getSize()<< std::endl;
//      }
      EXPECT_TRUE(currentKey[i] >= bbxMinKey[i] && currentKey[i] <= bbxMaxKey[i]);
    }

    bbxVoxels.insert(std::pair<OcTreeKey,double>(currentKey, it.getSize()));
  }
  EXPECT_EQ(bbxVoxels.size(), count);
  std::cout << "Bounding box traversed ("<< count << " leaf nodes)\n\n";


  // compare with manual BBX check on all leafs:
  for(OcTree::leaf_iterator it = tree->begin(), end=tree->end(); it!= end; ++it) {
    OcTreeKey key = it.getKey();
    if (    key[0] >= bbxMinKey[0] && key[0] <= bbxMaxKey[0]
         && key[1] >= bbxMinKey[1] && key[1] <= bbxMaxKey[1]
         && key[2] >= bbxMinKey[2] && key[2] <= bbxMaxKey[2])
    {
      KeyVolumeMap::iterator bbxIt = bbxVoxels.find(key);
      EXPECT_FALSE(bbxIt == bbxVoxels.end());
      EXPECT_TRUE(key == bbxIt->first);
      EXPECT_EQ(it.getSize(), bbxIt->second);
    }

  }


  std::cout << "Tests successful\n";


  return 0;
}

