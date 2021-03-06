// Specify a diameter range (tmin, tmax) and find all vessels (edges) with all nodes having
// diameter within this range.
// Working with the Amira spatialgraph file format.

#include <cstdio>
#include <vector>

#include <algorithm>
#include <math.h>
#include <string.h>
#include <string>
#include <sstream>
#include <assert.h>
#include <ctime>

#include "network.h"

FILE *fperr, *fpout;
int nconnected;
int ne_net[NEMAX];
float diam_min, diam_max, origin_shift[3];
float len_min, len_max, len_limit, ratio;
bool use_diameter, use_len_limit, use_ratio;
bool less_ratio;

float ddiam, dlen;

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
float dist(NETWORK *net, int k1, int k2)
{
	float dx = net->point[k2].x - net->point[k1].x;
	float dy = net->point[k2].y - net->point[k1].y;
	float dz = net->point[k2].z - net->point[k1].z;
	return sqrt(dx*dx+dy*dy+dz*dz);
}

//-----------------------------------------------------------------------------------
// mode = 'N' for numbers
//        'D' for diameters
//        'P' for positions
//-----------------------------------------------------------------------------------
void showedge(NETWORK *net, int ie, char mode)
{
	int k, kp;

	printf("edge: %d npts: %d\n",ie,net->edgeList[ie].npts);
	for (k=0; k<net->edgeList[ie].npts; k++) {
		kp = net->edgeList[ie].pt[k];
		if (mode == 'N')
			printf("%7d ",kp);
		else if (mode == 'D')
			printf("%7.1f ",net->point[kp].d);
		else 
			printf("%7.1f %7.1f %7.1f   ",net->point[kp].x,net->point[kp].y,net->point[kp].z);
		printf("\n");
	}
}

//-----------------------------------------------------------------------------------------------------
// This code is faulty - because points can appear multiple times, there are multiple subtractions.
// NOT USED
//-----------------------------------------------------------------------------------------------------
int ShiftOrigin(NETWORK *net, float origin_shift[])
{
	int i, k, j;
	EDGE edge;

	for (i=0;i<net->nv;i++) {
		net->vertex[i].point.x -= origin_shift[0];
		net->vertex[i].point.y -= origin_shift[1];
		net->vertex[i].point.z -= origin_shift[2];
	}
	for (i=0;i<net->ne;i++) {
		edge = net->edgeList[i];
		for (k=0;k<edge.npts;k++) {
			j = edge.pt[k];
			net->point[j].x -= origin_shift[0];
			net->point[j].y -= origin_shift[1];
			net->point[j].z -= origin_shift[2];
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------------------------------
// Check for vertex index iv in ivlist.  If it exists, return the index. 
// Otherwise add it to the list, increment nv, return the index.
//-----------------------------------------------------------------------------------------------------
int ivlistAdd(int iv, int *ivlist, int *nv)
{
	if (*nv == 0) {
		ivlist[0] = iv;
		*nv = 1;
		return 0;
	}
	for (int i=0; i<*nv; i++) {
		if (iv == ivlist[i]) {
			return i;
		}
	}
	ivlist[*nv] = iv;
	(*nv)++;
	return *nv-1;
}

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
int adjacentVertex(NETWORK *net, int kv, int ie)
{
	if (kv == net->edgeList[ie].vert[0])
		return net->edgeList[ie].vert[1];
	else
		return net->edgeList[ie].vert[0];
}

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
int getStartingEdge(NETWORK *net)
{
	int ie;
	for (ie=0; ie<net->ne; ie++) {
		if (net->edgeList[ie].netID == 0) {
			return ie;
		}
	}
	return -1;
}

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
//procedure DFS(G,v):
int DFS(NETWORK *net, int kv, int netID)
{
	int i, ie, kw;

    //label v as explored
	net->vertex[kv].netID = netID;
    //for all edges e in G.adjacentEdges(v) do
	for (i=0; i<net->vertex[kv].nlinks;i++) {
		ie = net->vertex[kv].edge[i];
    //    if edge e is unexplored then
		if (net->edgeList[ie].netID == 0) {
    //        w <- G.adjacentVertex(v,e)
			kw = adjacentVertex(net,kv,ie);
    //        if vertex w is unexplored then
			if (net->vertex[kw].netID == 0) {
    //            label e as a discovery edge
				net->edgeList[ie].netID = netID;	// no distinction between discovery and back edges
				nconnected++;
    //            recursively call DFS(G,w)
				DFS(net,kw,netID);
    //        else
			} else {
    //            label e as a back edge
				net->edgeList[ie].netID = netID;	// no distinction between discovery and back edges
				nconnected++;
			}
		}
	}
	return 0;
}

//-----------------------------------------------------------------------------------------------------
// All edges connected to edge ie0 are tagged with netID.
// Returns the number of tagged edges as nconnected.
// This is the kernel of the program.
// Here is the pseudocode from Wikipedia Depth-first search
//procedure DFS(G,v):
//    label v as explored
//    for all edges e in G.adjacentEdges(v) do
//        if edge e is unexplored then
//            w <- G.adjacentVertex(v,e)
//            if vertex w is unexplored then
//                label e as a discovery edge
//                recursively call DFS(G,w)
//            else
//                label e as a back edge	
//-----------------------------------------------------------------------------------------------------
int ConnectEdges(NETWORK *net, int kv0, int netID, int *nconn)
{
	nconnected = 0;
	printf("kv0, netID: %d %d\n",kv0,netID);
	DFS(net,kv0,netID);
	*nconn = nconnected;
	return 0;
}

//-----------------------------------------------------------------------------------------------------
// Network net1 is created from the edges of net0 that are tagged with netID
// Note the inefficiency in keeping all points, used or not.
//-----------------------------------------------------------------------------------------------------
int ExtractConnectedNet(NETWORK *net0, NETWORK *net1, int netID)
{
	int i, j, ie, iv, kp, kv, ne, nv;
	EDGE *elist;
	int *ivlist;

	ne = 0;
	for (i=0; i<net0->ne; i++) {
		if (net0->edgeList[i].netID == netID) ne++;
	}
	printf("Connected: ne: %d\n",ne);
	// Create a list of edges, and a list of vertices
	// The index number of a vertex in the list replaces vert[] in the elist entry.
	elist = (EDGE *)malloc(ne*sizeof(EDGE));
	ivlist = (int *)malloc(2*ne*sizeof(int));
	nv = 0;
	ne = 0;
	for (i=0; i<net0->ne; i++) {

		if (net0->edgeList[i].netID == netID) {
			elist[ne] = net0->edgeList[i];
			for (j=0; j<2; j++) {
				iv = net0->edgeList[i].vert[j];
				kv = ivlistAdd(iv,ivlist,&nv);
				elist[ne].vert[j] = kv;
			}
			ne++;
		}
	}
	printf("Connected: ne: %d  nv: %d\n",ne,nv);
	// Now from this list of edges we need to create the network net1.
	// Note that the vertex indices in ivlist[] are for the net0 vertex list.
	net1->vertex = (VERTEX *)malloc(nv*sizeof(VERTEX));
	net1->edgeList = (EDGE *)malloc(ne*sizeof(EDGE));
	net1->point = (POINT *)malloc(net0->np*sizeof(POINT));
	// Set up net1 vertices
	for (iv=0; iv<nv; iv++) {
		net1->vertex[iv] = net0->vertex[ivlist[iv]];
		net1->vertex[iv].netID = 0;
	}
	// Set up net1 edges
	for (i=0; i<ne; i++) {
		net1->edgeList[i] = elist[i];
		net1->edgeList[i].netID = 0;	// initially edges are not connected to a con_net
	}
	// Copy net0 points to net1, initially set all points unused
	for (i=0; i<net0->np; i++) {
		net1->point[i] = net0->point[i];
		net1->point[i].used = false;
	}
	// Now flag those points that are used.
	for (ie=0; ie<ne; ie++) {
		for (kp=0; kp<elist[ie].npts; kp++) {
			i = elist[ie].pt[kp];
			net1->point[i].used = true;
			//if (net1->point[i].d > 75) {
			//	printf("Error ExtractConnectedNet: edge: %d npts: %d pt: %d %d d: %6.1f\n",ie,elist[ie].npts,kp,i,net1->point[i].d);
			//	return 1;
			//}
		}
	}

	net1->ne = ne;
	net1->np = net0->np;
	net1->nv = nv;
	return 0;
}

//-----------------------------------------------------------------------------------------------------
//-----------------------------------------------------------------------------------------------------
int CreateLargestConnectedNet(NETWORK *net1, NETWORK *net2)
{
	int ie0, kv0, netID, nconn, err;
	int ne_max, netID_max;

	// First determine a net_ID for every edge
	netID = 0;
	for (;;) {
		// Start from any unconnected edge
		ie0 = getStartingEdge(net1);
		if (ie0 < 0) break;	// done
		netID++;
		if (netID > NEMAX) {
			printf("NEMAX exceeded\n");
			return 1;
		}
		kv0 = net1->edgeList[ie0].vert[0];
		printf("netID, ie0, kv0: %d %d %d\n",netID,ie0,kv0);
		ConnectEdges(net1,kv0,netID,&nconn);
		ne_net[netID] = nconn;
		printf("netID, nconn: %d %d\n",netID,nconn);
	}
	ne_max = 0;
	for (int i=1; i<=netID; i++) {
		if (ne_net[i] > ne_max) {
			ne_max = ne_net[i];
			netID_max = i;
		}
	}
	// Now create net2 from the largest subnetwork
	err = ExtractConnectedNet(net1,net2,netID_max);
	if (err != 0) return err;
	return 0;
}

//-----------------------------------------------------------------------------------------------------
// Locate all edges with diameter in a specified range.
// The criterion is either:
// diam_flag = 0  average
//             1  majority
//             2  all
// Note the inefficiency in keeping all points, used or not.
//-----------------------------------------------------------------------------------------------------
int CreateDiamSelectNet(NETWORK *net0, NETWORK *net1, float diam_min, float diam_max, int diam_flag)
{
	int i, j, ie, iv, kp, kv, ne, nv, nin, kin;
	float d, dave;
	bool in;
	EDGE *elist;
	int *ivlist;

	ne = 0;
	for (i=0; i<net0->ne; i++) {
		dave = 0;
		kin = 0;
		nin = net0->edgeList[i].npts/2;
		for (j=0; j<net0->edgeList[i].npts; j++) {
			kp = net0->edgeList[i].pt[j];
			d = net0->point[kp].d;
			dave += d;
			if (d >= diam_min && d < diam_max) {
				kin++;
			}
		}
		if (diam_flag == 0) {
			dave = dave/net0->edgeList[i].npts;
			if (dave < diam_min || dave > diam_max) {
				in = false;
			} else {
				in = true;
			}
		} else if (diam_flag == 1) {
			if (kin > nin) {
				in = true;
			} else {
				in = false;
			}
		} else if (diam_flag == 2) {
			if (kin == net0->edgeList[i].npts) {
				in = true;
			} else {
				in = false;
			}
		}
		if (in) ne++;
	}
	printf("ne: %d\n",ne);
	// Create a list of edges, and a list of vertices
	// The index number of a vertex in the list replaces vert[] in the elist entry.
	elist = (EDGE *)malloc(ne*sizeof(EDGE));
	ivlist = (int *)malloc(2*ne*sizeof(int));
	nv = 0;
	ne = 0;
	for (i=0; i<net0->ne; i++) {
		dave = 0;
		kin = 0;
		nin = net0->edgeList[i].npts/2;
		for (j=0; j<net0->edgeList[i].npts; j++) {
			kp = net0->edgeList[i].pt[j];
			d = net0->point[kp].d;
			dave += d;
			if (d >= diam_min && d < diam_max) {
				kin++;
			}
		}
		if (diam_flag == 0) {
			dave = dave/net0->edgeList[i].npts;
			if (dave < diam_min || dave > diam_max) {
				in = false;
			} else {
				in = true;
			}
		} else if (diam_flag == 1) {
			if (kin > nin) {
				in = true;
			} else {
				in = false;
			}
		} else if (diam_flag == 2) {
			if (kin == net0->edgeList[i].npts) {
				in = true;
			} else {
				in = false;
			}
		}	
		/*
		if (use_average) {
			dave = dave/net0->edgeList[i].npts;
			if (dave < diam_min || dave > diam_max) {
				in = false;
			} else {
				in = true;
			}
		} else {
			if (kin > nin) {
				in = true;
			} else {
				in = false;
			}
		}
		*/
		if (in) {
			elist[ne] = net0->edgeList[i];
			for (j=0; j<2; j++) {
				iv = net0->edgeList[i].vert[j];
				kv = ivlistAdd(iv,ivlist,&nv);
				elist[ne].vert[j] = kv;
			}
			ne++;
		}
	}
	printf("ne: %d  nv: %d\n",ne,nv);
	// Now from this list of edges we need to create the network net1.
	// Note that the vertex indices in ivlist[] are for the net0 vertex list.
	net1->vertex = (VERTEX *)malloc(nv*sizeof(VERTEX));
	net1->edgeList = (EDGE *)malloc(ne*sizeof(EDGE));
	net1->point = (POINT *)malloc(net0->np*sizeof(POINT));	// keeping full original list of points
	// Set up net1 vertices
	for (iv=0; iv<nv; iv++) {
		net1->vertex[iv] = net0->vertex[ivlist[iv]];
		net1->vertex[iv].nlinks = 0;
	}
	// Set up net1 edges and count vertex edges
	for (i=0; i<ne; i++) {
		net1->edgeList[i] = elist[i];
		net1->edgeList[i].netID = 0;	// initially edges are not connected to a connected subnetwork
		for (int k=0; k<2;k++) {
			iv = net1->edgeList[i].vert[k];
			net1->vertex[iv].nlinks++;
		}
	}
	// Set up vertex edge lists
	for (iv=0; iv<nv; iv++) {
		net1->vertex[iv].edge = (int *)malloc(net1->vertex[iv].nlinks*sizeof(int));
		net1->vertex[iv].nlinks = 0;
	}
	for (i=0; i<ne; i++) {
		for (int k=0; k<2;k++) {
			iv = net1->edgeList[i].vert[k];
			net1->vertex[iv].edge[net1->vertex[iv].nlinks] = i;
			net1->vertex[iv].nlinks++;
		}
	}

	// Copy net0 points to net1, initially set all points unused
	for (i=0; i<net0->np; i++) {
		net1->point[i] = net0->point[i];
		net1->point[i].used = false;
	}
	// Now flag those points that are used.
	for (ie=0; ie<ne; ie++) {
		for (kp=0; kp<elist[ie].npts; kp++) {
			i = elist[ie].pt[kp];
			net1->point[i].used = true;
		}
	}

	net1->ne = ne;
	net1->np = net0->np;
	net1->nv = nv;
	return 0;
}

//-----------------------------------------------------------------------------------------------------
// Locate all edges with length in a specified range.
//-----------------------------------------------------------------------------------------------------
int CreateLenSelectNet(NETWORK *net0, NETWORK *net1, float len_min, float len_max)
{
	int i, j, ie, iv, kp, kp1, kp2, kv, ne, nv;
	float len;
	bool in;
	EDGE *elist;
	int *ivlist;

	fprintf(fpout,"Number of edges: %d\n",net0->ne);
	ne = 0;
	for (i=0; i<net0->ne; i++) {
		len = 0;
		for (j=1; j<net0->edgeList[i].npts; j++) {
			kp2 = net0->edgeList[i].pt[j];
			kp1 = net0->edgeList[i].pt[j-1];
			//dx = net0->point[kp2].x - net0->point[kp1].x;
			//dy = net0->point[kp2].y - net0->point[kp1].y;
			//dz = net0->point[kp2].z - net0->point[kp1].z;
			//len += sqrt(dx*dx + dy*dy + dz*dz);
			len += dist(net0,kp1,kp2);
		}
		if (len < len_min || len > len_max) {
			in = false;
//			fprintf(fpout,"edge: %6d npts: %3d len: %6.1f\n",i,net0->edgeList[i].npts,len);
		} else {
			in = true;
		}
		if (in) ne++;
	}
	printf("number in the length range: %d  number outside: %d\n",ne,net0->ne-ne);
	fprintf(fpout,"number in the length range: %d  number outside: %d\n",ne,net0->ne-ne);
	fprintf(fpout,"Number of edges: %d\n",net0->ne);
	// Create a list of edges, and a list of vertices
	// The index number of a vertex in the list replaces vert[] in the elist entry.
	elist = (EDGE *)malloc(ne*sizeof(EDGE));
	ivlist = (int *)malloc(2*ne*sizeof(int));
	nv = 0;
	ne = 0;
	for (i=0; i<net0->ne; i++) {
		len = 0;
		for (j=1; j<net0->edgeList[i].npts; j++) {
			kp2 = net0->edgeList[i].pt[j];
			kp1 = net0->edgeList[i].pt[j-1];
			//dx = net0->point[kp2].x - net0->point[kp1].x;
			//dy = net0->point[kp2].y - net0->point[kp1].y;
			//dz = net0->point[kp2].z - net0->point[kp1].z;
			//len += sqrt(dx*dx + dy*dy + dz*dz);
			len += dist(net0,kp1,kp2);
		}
		if (len < len_min || len > len_max) {
			in = false;
		} else {
			in = true;
		}
		if (in) {
			elist[ne] = net0->edgeList[i];
			for (j=0; j<2; j++) {
				iv = net0->edgeList[i].vert[j];
				kv = ivlistAdd(iv,ivlist,&nv);
				elist[ne].vert[j] = kv;
			}
			ne++;
		}
	}
	printf("ne: %d  nv: %d\n",ne,nv);
	// Now from this list of edges we need to create the network net1.
	// Note that the vertex indices in ivlist[] are for the net0 vertex list.
	net1->vertex = (VERTEX *)malloc(nv*sizeof(VERTEX));
	net1->edgeList = (EDGE *)malloc(ne*sizeof(EDGE));
	net1->point = (POINT *)malloc(net0->np*sizeof(POINT));	// keeping full original list of points
	// Set up net1 vertices
	for (iv=0; iv<nv; iv++) {
		net1->vertex[iv] = net0->vertex[ivlist[iv]];
		net1->vertex[iv].nlinks = 0;
	}
	// Set up net1 edges and count vertex edges
	for (i=0; i<ne; i++) {
		net1->edgeList[i] = elist[i];
		net1->edgeList[i].netID = 0;	// initially edges are not connected to a connected subnetwork
		for (int k=0; k<2;k++) {
			iv = net1->edgeList[i].vert[k];
			net1->vertex[iv].nlinks++;
		}
	}
	// Set up vertex edge lists
	for (iv=0; iv<nv; iv++) {
		net1->vertex[iv].edge = (int *)malloc(net1->vertex[iv].nlinks*sizeof(int));
		net1->vertex[iv].nlinks = 0;
	}
	for (i=0; i<ne; i++) {
		for (int k=0; k<2;k++) {
			iv = net1->edgeList[i].vert[k];
			net1->vertex[iv].edge[net1->vertex[iv].nlinks] = i;
			net1->vertex[iv].nlinks++;
		}
	}

	// Copy net0 points to net1, initially set all points unused
	for (i=0; i<net0->np; i++) {
		net1->point[i] = net0->point[i];
		net1->point[i].used = false;
	}
	// Now flag those points that are used.
	for (ie=0; ie<ne; ie++) {
		for (kp=0; kp<elist[ie].npts; kp++) {
			i = elist[ie].pt[kp];
			net1->point[i].used = true;
		}
	}

	net1->ne = ne;
	net1->np = net0->np;
	net1->nv = nv;
	return 0;
}

//-----------------------------------------------------------------------------------------------------
// Locate all edges with length/diameter in a specified range, either < ratio or > ratio.
//-----------------------------------------------------------------------------------------------------
int CreateLRatioSelectNet(NETWORK *net0, NETWORK *net1, float ratio, bool less_ratio)
{
	int i, j, ie, iv, kp, kp1, kp2, kv, ne, nv, kin;
	float len, d, dave, rat;
	bool in;
	EDGE *elist;
	int *ivlist;

	printf("CreateLRatioSelectNet: Number of edges: %d less_ratio flag: %d\n",net0->ne,less_ratio);
	fprintf(fpout,"CreateLRatioSelectNet: Number of edges: %d less_ratio: %d\n",net0->ne,less_ratio);
	ne = 0;
	for (i=0; i<net0->ne; i++) {
		len = 0;
		kin = 0;
		dave = net0->point[net0->edgeList[i].pt[0]].d;
		for (j=1; j<net0->edgeList[i].npts; j++) {
			kp2 = net0->edgeList[i].pt[j];
			kp1 = net0->edgeList[i].pt[j-1];
			len += dist(net0,kp1,kp2);
			d = net0->point[kp2].d;
			dave += d;
		}
		dave /= net0->edgeList[i].npts;
		rat = len/dave;
		in = (less_ratio && rat <= ratio) || (!less_ratio && rat > ratio);
		if (in) ne++;
	}
	printf("number in the ratio range: %d  number outside: %d\n",ne,net0->ne-ne);
	fprintf(fpout,"number in the ratio range: %d  number outside: %d\n",ne,net0->ne-ne);
	fflush(stdout);
	fflush(fpout);
	// Create a list of edges, and a list of vertices
	// The index number of a vertex in the list replaces vert[] in the elist entry.
	elist = (EDGE *)malloc(ne*sizeof(EDGE));
	ivlist = (int *)malloc(2*ne*sizeof(int));
	nv = 0;
	ne = 0;
	for (i=0; i<net0->ne; i++) {
		len = 0;
		kin = 0;
		dave = net0->point[net0->edgeList[i].pt[0]].d;
		for (j=1; j<net0->edgeList[i].npts; j++) {
			kp2 = net0->edgeList[i].pt[j];
			kp1 = net0->edgeList[i].pt[j-1];
			len += dist(net0,kp1,kp2);
			d = net0->point[kp2].d;
			dave += d;
		}
		dave /= net0->edgeList[i].npts;
		rat = len/dave;
		in = (less_ratio && rat <= ratio) || (!less_ratio && rat > ratio);
		if (in) {
			elist[ne] = net0->edgeList[i];
			for (j=0; j<2; j++) {
				iv = net0->edgeList[i].vert[j];
				kv = ivlistAdd(iv,ivlist,&nv);
				elist[ne].vert[j] = kv;
			}
			ne++;
		}
	}
	printf("ne: %d  nv: %d\n",ne,nv);
	// Now from this list of edges we need to create the network net1.
	// Note that the vertex indices in ivlist[] are for the net0 vertex list.
	net1->vertex = (VERTEX *)malloc(nv*sizeof(VERTEX));
	net1->edgeList = (EDGE *)malloc(ne*sizeof(EDGE));
	net1->point = (POINT *)malloc(net0->np*sizeof(POINT));	// keeping full original list of points
	// Set up net1 vertices
	for (iv=0; iv<nv; iv++) {
		net1->vertex[iv] = net0->vertex[ivlist[iv]];
		net1->vertex[iv].nlinks = 0;
	}
	// Set up net1 edges and count vertex edges
	for (i=0; i<ne; i++) {
		net1->edgeList[i] = elist[i];
		net1->edgeList[i].netID = 0;	// initially edges are not connected to a connected subnetwork
		for (int k=0; k<2;k++) {
			iv = net1->edgeList[i].vert[k];
			net1->vertex[iv].nlinks++;
		}
	}
	// Set up vertex edge lists
	for (iv=0; iv<nv; iv++) {
		net1->vertex[iv].edge = (int *)malloc(net1->vertex[iv].nlinks*sizeof(int));
		net1->vertex[iv].nlinks = 0;
	}
	for (i=0; i<ne; i++) {
		for (int k=0; k<2;k++) {
			iv = net1->edgeList[i].vert[k];
			net1->vertex[iv].edge[net1->vertex[iv].nlinks] = i;
			net1->vertex[iv].nlinks++;
		}
	}

	// Copy net0 points to net1, initially set all points unused
	for (i=0; i<net0->np; i++) {
		net1->point[i] = net0->point[i];
		net1->point[i].used = false;
	}
	// Now flag those points that are used.
	for (ie=0; ie<ne; ie++) {
		for (kp=0; kp<elist[ie].npts; kp++) {
			i = elist[ie].pt[kp];
			net1->point[i].used = true;
		}
	}

	net1->ne = ne;
	net1->np = net0->np;
	net1->nv = nv;
	return 0;
}

int myFactorial( int integer)
{
	if ( integer == 1)
		return 1;
	else {
		return (integer * (myFactorial(integer-1)));
	}
} 

//-----------------------------------------------------------------------------------------------------
// Find vertex nodes with only two edges connected
//-----------------------------------------------------------------------------------------------------
int CheckNetwork1(NETWORK *net)
{
	int iv, i, ne, nv, n2;

	fprintf(fpout,"CheckNetwork\n");
	nv = net->nv;
	ne = net->ne;
	for (iv=0; iv<nv; iv++) {
		net->vertex[iv].nlinks = 0;
	}
	// Set up net1 edges and count vertex edges
	for (i=0; i<ne; i++) {
		for (int k=0; k<2;k++) {
			iv = net->edgeList[i].vert[k];
			net->vertex[iv].nlinks++;
		}
	}
	n2 = 0;
	for (iv=0; iv<nv; iv++) {
		if (net->vertex[iv].nlinks == 2) {
			n2++;
			fprintf(fpout,"vertex: %6d nlinks: %2d\n",iv,net->vertex[iv].nlinks);
		}
	}
	fprintf(fpout,"Number of 2-link vertices: %d\n",n2);
	return 0;
	/*
	// Set up vertex edge lists
	for (iv=0; iv<nv; iv++) {
		net->vertex[iv].edge = (int *)malloc(net->vertex[iv].nlinks*sizeof(int));
		net->vertex[iv].nlinks = 0;
	}
	for (i=0; i<ne; i++) {
		for (int k=0; k<2;k++) {
			iv = net->edgeList[i].vert[k];
			net->vertex[iv].edge[net->vertex[iv].nlinks] = i;
			net->vertex[iv].nlinks++;
		}
	}
	*/
}

/*
//-----------------------------------------------------------------------------------------------------
// The average diameter of a vessel (edge) is now estimated by dividing the volume by the length.
// All distances in the .am file are in um.
//-----------------------------------------------------------------------------------------------------
int CreateDistributions1(NETWORK *net)
{
	int adbox[NBOX], lvbox[NBOX];
	int segadbox[NBOX];
	double lsegadbox[NBOX];
	double ad, len, ddiam, dlen, ltot, lsum, dsum, dvol, r2, r2prev, lsegdtot;
	double ave_len, volume, d95;
	double ave_pt_diam, ave_seg_diam;
	int ie, ip, k, ka, kp, kpprev, ndpts, nlpts, ndtot, nsegdtot;
	double lenlimit = 3.0;
	EDGE edge;

	for (k=0;k<NBOX;k++) {
		adbox[k] = 0;
		segadbox[k] = 0;
		lsegadbox[k] = 0;
		lvbox[k] = 0;
	}
	printf("Compute diameter distributions\n");
	fprintf(fperr,"Compute diameter distributions\n");
	// Diameters
	ddiam = 0.5;
	ndtot = 0;
	nsegdtot = 0;
	lsegdtot = 0;
	ave_pt_diam = 0;
	ave_seg_diam = 0;
	volume = 0;
	for (ie=0; ie<net->ne; ie++) {
		edge = net->edgeList[ie];
		if (!edge.used) continue;
//		printf("ie: %d npts: %d\n",ie,edge.npts);
		fprintf(fperr,"ie: %d npts: %d\n",ie,edge.npts);
		fflush(fperr);
		bool dbug = false;
		kpprev = 0;
		r2prev = 0;
		dsum = 0;
		lsum = 0;
		dvol = 0;
		for (ip=0; ip<edge.npts; ip++) {
			kp = edge.pt[ip];
			ad = net->point[kp].d;
//			ad = avediameter[kp];
			r2 = ad*ad/4;
			ave_pt_diam += ad;
			if (dbug) {
				printf("%d  %d  %f  %f\n",ip,kp,ad,ddiam);
				fprintf(fperr,"%d  %d  %f  %f\n",ip,kp,ad,ddiam);
			}
			fflush(fperr);
//			dsum += ad;
			if (ad < 0.001) {
				printf("Zero point diameter: edge: %d point: %d ad: %f\n",ie,ip,ad);
				fprintf(fperr,"Zero point diameter: edge: %d point: %d ad: %f\n",ie,ip,ad);
				return 1;
			}
			ka = int(ad/ddiam + 0.5);
			if (ka >= NBOX) {
				printf("Vessel too wide (point): d: %f k: %d\n",ad,ka);
				fprintf(fperr,"Vessel too wide (point): d: %f k: %d\n",ad,ka);
				continue;
			}
			adbox[ka]++;
			ndtot++;
			if (ip > 0) {
				dlen = dist(net,kp,kpprev);
				dvol += PI*dlen*(r2 + r2prev)/2;
				lsum += dlen;
			}
			kpprev = kp;
			r2prev = r2;
		}
		net->edgeList[ie].length_um = float(lsum);
		volume += dvol;
		if (dbug) {
			printf("lsum: %f\n",lsum);
			fprintf(fperr,"lsum: %f\n",lsum);
			fflush(fperr);
			if (lsum == 0) return 1;
		}
//		ad = dsum/edge.npts;
		ad = 2*sqrt(dvol/(PI*lsum));	// segment diameter
		ave_seg_diam += ad;
		if (ad < 0.001) {
			printf("Zero segment diameter: edge: %d ad: %f\n",ie,ad);
			fprintf(fperr,"Zero segment diameter: edge: %d ad: %f\n",ie,ad);
			return 1;
		}
		ka = int(ad/ddiam + 0.5);
		if (ka >= NBOX) {
			printf("Vessel too wide (segment ave): d: %f k: %d\n",ad,ka);
			fprintf(fperr,"Vessel too wide (segment ave): d: %f k: %d\n",ad,ka);
			continue;
		}
		segadbox[ka]++;
		nsegdtot++;
		lsegadbox[ka] += lsum;
		lsegdtot += lsum;
	}
	// Determine d95, the diameter that >95% of points exceed.
	dsum = 0;
	for (k=0; k<NBOX; k++) {
		dsum += adbox[k]/float(ndtot);
		if (dsum > 0.05) {
			d95 = (k-1)*ddiam;
			break;
		}
	}
	printf("Compute length distributions: lower limit = %6.1f um\n",lenlimit);
	fprintf(fperr,"Compute length distributions: lower limit = %6.1f um\n",lenlimit);
	// Lengths
	dlen = 1;
	ltot = 0;
	ave_len = 0;
	for (ie=0; ie<net->ne; ie++) {
		edge = net->edgeList[ie];
		if (!edge.used) continue;
		len = edge.length_um;
		k = int(len/dlen + 0.5);
		if (k*dlen <= lenlimit) continue;
		if (k >= NBOX) {
			printf("Edge too long: len: %d  %f  k: %d\n",ie,len,k);
			fprintf(fperr,"Edge too long: len: %d  %f  k: %d\n",ie,len,k);
			continue;
		}
		lvbox[k]++;
		ave_len += len;
		ltot++;
	}
	ave_pt_diam /= ndtot;
	ave_seg_diam /= nsegdtot;
	fprintf(fpout,"Total vertices: %d  points: %d\n",net->nv,net->np);
	fprintf(fpout,"Vessels: %d\n",net->ne);
	printf("Average pt diameter: %6.2f vessel diameter: %6.2f\n",ave_pt_diam, ave_seg_diam);
	fprintf(fpout,"Average pt diameter: %6.2f vessel diameter: %6.2f\n",ave_pt_diam, ave_seg_diam);
	printf("Average vessel length: %6.1f\n",ave_len/ltot);
	fprintf(fpout,"Average vessel length: %6.1f\n",ave_len/ltot);
	fprintf(fpout,"Volume: %10.0f\n\n",volume);

	//ndpts = 0;
	//for (k=0; k<NBOX; k++) {
	//	if (adbox[k]/float(ndtot) >= 0.0005) {
	//		ndpts = k+1;
	//	}
	//}
	for (k=NBOX-1; k>=0; k--) {
		if (segadbox[k] > 0) break;
	}
	ndpts = k+2;
	fprintf(fpout,"Vessel diameter distribution\n");
	fprintf(fpout,"   um    number  fraction    length  fraction\n");
	for (k=0; k<ndpts; k++) {
		fprintf(fpout,"%6.2f %8d %9.5f  %8.0f %9.5f\n",k*ddiam,segadbox[k],segadbox[k]/float(nsegdtot),
			lsegadbox[k],lsegadbox[k]/lsegdtot);
	}

	//nlpts = 0;
	//for (k=0; k<NBOX; k++) {
	//	if (lvbox[k]/ltot >= 0.0005) {
	//		nlpts = k+1;
	//	}
	//}
	for (k=NBOX-1; k>=0; k--) {
		if (lvbox[k] > 0) break;
	}
	nlpts = k+2;
	fprintf(fpout,"Vessel length distribution\n");
	fprintf(fpout,"   um    number  fraction\n");
	for (k=0; k<nlpts; k++) {
		fprintf(fpout,"%6.2f %8d %9.5f\n",k*dlen,lvbox[k],lvbox[k]/ltot);
	}
	return 0;
}
*/

//-----------------------------------------------------------------------------------------------------
// If connect_flag==1, the largest connected network is found, other edges are dropped.
//-----------------------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
	int err;
	char *input_amfile;
	char drive[32], dir[2048],filename[256], ext[32];
	char errfilename[2048], output_amfile[2048], result_file[2048];
	char output_basename[2048];
	int diam_flag, cmgui_flag, connect_flag, mode_flag;
	float val_min, val_max;
	bool connect;
	NETWORK *NP0, *NP1, *NP2;

	if (argc != 9) {
		printf("Usage: select input_amfile output_amfile mode val_min val_max connect_flag diam_flag cmgui_flag\n");
		printf("       mode = 0 for length selection, = 1 for diameter selection, = 2 for length/diameter ratio selection\n");
		printf("       if mode = 0 or 1, the range is (val_min,val_max)\n");
		printf("       if mode = 2, length/diameter ratio = val_min, select < ratio if val_max < 0, > ratio if val_max > 0\n");
		printf("       diam_flag = 0  average of points on the edge\n");
        printf("                 = 1  majority ....................\n");
        printf("                 = 2  all .........................\n");
		fperr = fopen("select_error.log","w");
		fprintf(fperr,"Usage: select input_amfile output_amfile mode val_min val_max connect_flag diam_flag cmgui_flag\n");
		fprintf(fperr,"       mode = 0 for length selection, = 1 for diameter selection, = 2 for length/diameter ratio selection\n");
		fprintf(fperr,"       if mode = 0 or 1, the range is (val_min,val_max)\n");
		fprintf(fperr,"       if mode = 2, length/diameter ratio = val_min, select < ratio if val_max < 0, > ratio if val_max > 0\n");
		fprintf(fperr,"       diam_flag = 0  average of points on the edge\n");
        fprintf(fperr,"                 = 1  majority ....................\n");
        fprintf(fperr,"                 = 2  all .........................\n");
		fprintf(fperr,"Submitted command line: argc: %d\n",argc);
		for (int i=0; i<argc; i++) {
			fprintf(fperr,"argv: %d: %s\n",i,argv[i]);
		}
		fclose(fperr);
		return 1;	// Wrong command line
	}

	input_amfile = argv[1];
	strcpy(output_amfile,argv[2]);
	sscanf(argv[3],"%d",&mode_flag);
	sscanf(argv[4],"%f",&val_min);
	sscanf(argv[5],"%f",&val_max);
	sscanf(argv[6],"%d",&connect_flag);
	sscanf(argv[7],"%d",&diam_flag);
	sscanf(argv[8],"%d",&cmgui_flag);
	connect = (connect_flag != 0);
	use_len_limit = (mode_flag == 0);
	use_diameter = (mode_flag == 1);
	use_ratio = (mode_flag == 2);
	if (use_diameter) {
		diam_min = val_min;
		diam_max = val_max;
	}
	if (use_len_limit) {
		len_min = val_min;
		len_max = val_max;
	}
	if (use_ratio) {
		ratio = val_min;
		less_ratio = (val_max < 0);
	}
	ddiam = 0.5;
	dlen = 1.0;

	_splitpath(output_amfile,drive,dir,filename,ext);
	strcpy(output_basename,drive);
	strcat(output_basename,dir);
	strcat(output_basename,filename);
	sprintf(errfilename,"%s_select.log",output_basename);
	sprintf(result_file,"%s_select.out",output_basename);
	fperr = fopen(errfilename,"w");

//	fprintf(fperr,"drive: %s dir: %s filename: %s ext: %s\n",drive,dir,filename,ext);
//	fprintf(fperr,"Basename: %s\n",output_basename);

	fpout = fopen(result_file,"w");	
	printf("Input .am file: %s\n",input_amfile);
	printf("mode_flag: %d\n",mode_flag);
	printf("connect_flag: %d\n",connect_flag);
	printf("diam_flag: %d\n",diam_flag);
	printf("cmgui_flag: %d\n",cmgui_flag);
	fprintf(fpout,"Input .am file: %s\n",input_amfile);
	fprintf(fpout,"connect_flag: %d\n",connect_flag);
	fprintf(fpout,"diam_flag: %d\n",diam_flag);
	fprintf(fpout,"cmgui_flag: %d\n",cmgui_flag);
	if (use_diameter) {
		printf("Using diameter range:\n");
		printf("diam_min, diam_max: %6.2f %6.2f\n",diam_min,diam_max);
		fprintf(fpout,"Using diameter range:\n");
		fprintf(fpout,"diam_min, diam_max: %6.2f %6.2f\n",diam_min,diam_max);
	} else if (use_len_limit) {
		printf("Using length range:\n");
		printf("len_min, len_max: %6.1f %6.1f\n",len_min,len_max);
		fprintf(fpout,"Using length range:\n");
		fprintf(fpout,"len_min, len_max: %6.1f %6.1f\n",len_min,len_max);
	} else {
		printf("Using length/diameter limit:\n");
		fprintf(fpout,"Using length/diameter limit:\n");
		if (less_ratio) {
			printf("ratio < len_min: %6.1f\n",ratio);
			fprintf(fpout,"ratio < len_min: %6.1f\n",ratio);
		} else {
			printf("ratio > len_min: %6.1f\n",ratio);
			fprintf(fpout,"ratio > len_min: %6.1f\n",ratio);
		}
	}
	NP0 = (NETWORK *)malloc(sizeof(NETWORK));
	err = ReadAmiraFile(input_amfile,NP0);
	if (err != 0) return 2;

	NP1 = (NETWORK *)malloc(sizeof(NETWORK));
	if (use_diameter) {
		err = CreateDiamSelectNet(NP0,NP1,diam_min,diam_max,diam_flag);
		if (err != 0) return 3;
	} else if (use_len_limit) {
		err = CreateLenSelectNet(NP0,NP1,len_min,len_max);
		if (err != 0) return 4;
	} else if (use_ratio) {
		err = CreateLRatioSelectNet(NP0,NP1,ratio,less_ratio);
		if (err != 0) return 5;
	}
	printf("NP1: ne, nv, np: %d %d %d\n",NP1->ne,NP1->nv,NP1->np);

	if (connect) {
		NP2 = (NETWORK *)malloc(sizeof(NETWORK));
		err = CreateLargestConnectedNet(NP1,NP2);
		if (err != 0) return 6;
	}

	origin_shift[0] = 0;
	origin_shift[1] = 0;
	origin_shift[2] = 0;

	if (connect) {
		err = WriteAmiraFile(output_amfile,input_amfile,NP2,origin_shift);
		if (err != 0) return 7;
		if (cmgui_flag == 1) {
			err = WriteCmguiData(output_basename,NP2,origin_shift);
			if (err != 0) return 8;
		}
		err = EdgeDimensions(NP2->edgeList,NP2->point,NP2->ne);
		if (err != 0) return 9;
		err = CreateDistributions(NP2);
		if (err != 0) return 10;
	} else {
		err = WriteAmiraFile(output_amfile,input_amfile,NP1,origin_shift);
		if (err != 0) return 7;
		if (cmgui_flag == 1) {
			err = WriteCmguiData(output_basename,NP1,origin_shift);
			if (err != 0) return 8;
		}
		err = EdgeDimensions(NP1->edgeList,NP1->point,NP1->ne);
		if (err != 0) return 9;
		err = CreateDistributions(NP1);
		if (err != 0) return 10;
	}
	return 0;
}