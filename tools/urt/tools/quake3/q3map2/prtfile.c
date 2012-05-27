/* -------------------------------------------------------------------------------

This code is based on source provided under the terms of the Id Software 
LIMITED USE SOFTWARE LICENSE AGREEMENT, a copy of which is included with the
GtkRadiant sources (see LICENSE_ID). If you did not receive a copy of 
LICENSE_ID, please contact Id Software immediately at info@idsoftware.com.

All changes and additions to the original source which have been developed by
other contributors (see CONTRIBUTORS) are provided under the terms of the
license the contributors choose (see LICENSE), to the extent permitted by the
LICENSE_ID. If you did not receive a copy of the contributor license,
please contact the GtkRadiant maintainers at info@gtkradiant.com immediately.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------------------

This code has been altered significantly from its original form, to support
several games based on the Quake III Arena engine, in the form of "Q3Map2."

------------------------------------------------------------------------------- */



/* marker */
#define PRTFILE_C



/* dependencies */
#include "q3map2.h"



/*
==============================================================================

PORTAL FILE GENERATION

Save out name.prt for qvis to read
==============================================================================
*/


#define	PORTALFILE	"PRT1"

FILE	*pf;
int		num_visclusters;				// clusters the player can be in
int		num_visportals;
int		num_solidfaces;

void WriteFloat (FILE *f, vec_t v)
{
	if ( fabs(v - Q_rint(v)) < 0.001 )
		fprintf (f,"%i ",(int)Q_rint(v));
	else
		fprintf (f,"%f ",v);
}

/*
=================
WritePortalFile_r
=================
*/
void WritePortalFile_r (node_t *node)
{
	int			i, s;	
	portal_t	*p;
	winding_t	*w;
	vec3_t		normal;
	vec_t		dist;

	// decision node
	if (node->planenum != PLANENUM_LEAF) {
		WritePortalFile_r (node->children[0]);
		WritePortalFile_r (node->children[1]);
		return;
	}
	
	if (node->opaque) {
		return;
	}

	for (p = node->portals ; p ; p=p->next[s])
	{
		w = p->winding;
		s = (p->nodes[1] == node);
		if (w && p->nodes[0] == node)
		{
			if (!PortalPassable(p))
				continue;
			// write out to the file
			
			// sometimes planes get turned around when they are very near
			// the changeover point between different axis.  interpret the
			// plane the same way vis will, and flip the side orders if needed
			// FIXME: is this still relevent?
			WindingPlane (w, normal, &dist);
			if ( DotProduct (p->plane.normal, normal) < 0.99 )
			{	// backwards...
				fprintf (pf,"%i %i %i ",w->numpoints, p->nodes[1]->cluster, p->nodes[0]->cluster);
			}
			else
				fprintf (pf,"%i %i %i ",w->numpoints, p->nodes[0]->cluster, p->nodes[1]->cluster);
			
			/* ydnar: added this change to make antiportals work */
			if( p->compileFlags & C_HINT )
				fprintf( pf, "1 " );
			else
				fprintf( pf, "0 " );
			
			/* write the winding */
			for (i=0 ; i<w->numpoints ; i++)
			{
				fprintf (pf,"(");
				WriteFloat (pf, w->p[i][0]);
				WriteFloat (pf, w->p[i][1]);
				WriteFloat (pf, w->p[i][2]);
				fprintf (pf,") ");
			}
			fprintf (pf,"\n");
		}
	}

}

/*
=================
WriteFaceFile_r
=================
*/
void WriteFaceFile_r (node_t *node)
{
	int			i, s;	
	portal_t	*p;
	winding_t	*w;

	// decision node
	if (node->planenum != PLANENUM_LEAF) {
		WriteFaceFile_r (node->children[0]);
		WriteFaceFile_r (node->children[1]);
		return;
	}
	
	if (node->opaque) {
		return;
	}

	for (p = node->portals ; p ; p=p->next[s])
	{
		w = p->winding;
		s = (p->nodes[1] == node);
		if (w)
		{
			if (PortalPassable(p))
				continue;
			// write out to the file

			if (p->nodes[0] == node)
			{
				fprintf (pf,"%i %i ",w->numpoints, p->nodes[0]->cluster);
				for (i=0 ; i<w->numpoints ; i++)
				{
					fprintf (pf,"(");
					WriteFloat (pf, w->p[i][0]);
					WriteFloat (pf, w->p[i][1]);
					WriteFloat (pf, w->p[i][2]);
					fprintf (pf,") ");
				}
				fprintf (pf,"\n");
			}
			else
			{
				fprintf (pf,"%i %i ",w->numpoints, p->nodes[1]->cluster);
				for (i = w->numpoints-1; i >= 0; i--)
				{
					fprintf (pf,"(");
					WriteFloat (pf, w->p[i][0]);
					WriteFloat (pf, w->p[i][1]);
					WriteFloat (pf, w->p[i][2]);
					fprintf (pf,") ");
				}
				fprintf (pf,"\n");
			}
		}
	}
}

/*
================
NumberLeafs_r
================
*/
void NumberLeafs_r (node_t *node)
{
	portal_t	*p;

	if ( node->planenum != PLANENUM_LEAF ) {
		// decision node
		node->cluster = -99;
		NumberLeafs_r (node->children[0]);
		NumberLeafs_r (node->children[1]);
		return;
	}
	
	node->area = -1;

	if ( node->opaque ) {
		// solid block, viewpoint never inside
		node->cluster = -1;
		return;
	}

	node->cluster = num_visclusters;
	num_visclusters++;

	// count the portals
	for (p = node->portals ; p ; )
	{
		if (p->nodes[0] == node)		// only write out from first leaf
		{
			if (PortalPassable(p))
				num_visportals++;
			else
				num_solidfaces++;
			p = p->next[0];
		}
		else
		{
			if (!PortalPassable(p))
				num_solidfaces++;
			p = p->next[1];		
		}
	}
}


/*
================
NumberClusters
================
*/
void NumberClusters(tree_t *tree) {
	num_visclusters = 0;
	num_visportals = 0;
	num_solidfaces = 0;

	Sys_FPrintf (SYS_VRB,"--- NumberClusters ---\n");
	
	// set the cluster field in every leaf and count the total number of portals
	NumberLeafs_r (tree->headnode);

	Sys_FPrintf( SYS_VRB, "%9d visclusters\n", num_visclusters );
	Sys_FPrintf( SYS_VRB, "%9d visportals\n", num_visportals );
	Sys_FPrintf( SYS_VRB, "%9d solidfaces\n", num_solidfaces );
}

/*
================
WritePortalFile
================
*/
void WritePortalFile (tree_t *tree)
{
	char	filename[1024];

	Sys_FPrintf (SYS_VRB,"--- WritePortalFile ---\n");
	
	// write the file
	sprintf (filename, "%s.prt", source);
	Sys_Printf ("writing %s\n", filename);
	pf = fopen (filename, "w");
	if (!pf)
		Error ("Error opening %s", filename);
		
	fprintf (pf, "%s\n", PORTALFILE);
	fprintf (pf, "%i\n", num_visclusters);
	fprintf (pf, "%i\n", num_visportals);
	fprintf (pf, "%i\n", num_solidfaces);

	WritePortalFile_r(tree->headnode);
	WriteFaceFile_r(tree->headnode);

	fclose (pf);
}
