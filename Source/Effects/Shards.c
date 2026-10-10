/****************************/
/*   	SHARDS.C		    */
/* (c)2003 Pangea Software  */
/* By Brian Greenstone      */
/****************************/


/****************************/
/*    EXTERNALS             */
/****************************/

#include "game.h"

/****************************/
/*    PROTOTYPES            */
/****************************/

static int FindFreeShard(void);
static void ExplodeGeometry_Recurse(MetaObjectPtr obj);
static void ExplodeVertexArray(MOVertexArrayData *data, MOMaterialObject *overrideTexture);
static void MoveShards(ObjNode *theNode);
static void DrawShards(ObjNode *theNode);


/****************************/
/*    CONSTANTS             */
/****************************/

#define	MAX_SHARDS		2500

typedef struct
{
	Boolean					isUsed;
	OGLVector3D				rot,rotDelta;
	OGLPoint3D				coord,coordDelta;
	float					decaySpeed,scale;
	Byte					mode;
	OGLMatrix4x4			matrix;

	OGLPoint3D				points[3];
	OGLTextureCoord			uvs[3];
	MOMaterialObject		*material;
	OGLColorRGBA			colorFilter;
	uint32_t					glow;
}ShardType;


/*********************/
/*    VARIABLES      */
/*********************/

int			gNumShards = 0;
ShardType	gShards[MAX_SHARDS];

static	float		gBoomForce,gShardDecaySpeed;
static	Byte		gShardMode;
static 	long		gShardDensity;
static 	OGLMatrix4x4	gWorkMatrix;
static 	ObjNode		*gShardSrcObj;

/************************* INIT SHARD SYSTEM ***************************/

void InitShardSystem(void)
{
	gNumShards = 0;

	for (int i = 0; i < MAX_SHARDS; i++)
		gShards[i].isUsed = false;



			/* MAKE DUMMY OBJECT */

	NewObjectDefinitionType def =
	{
		.genre		= CUSTOM_GENRE,
		.slot		= PARTICLE_SLOT-1,
		.scale		= 1,
		.flags		= STATUS_BIT_DOUBLESIDED, //|STATUS_BIT_NOLIGHTING;
		.moveCall	= MoveShards,
		.drawCall	= DrawShards,
	};
	MakeNewObject(&def);
}




/********************* FIND FREE PARTICLE ***********************/
//
// OUTPUT: -1 == none free found
//

static int FindFreeShard(void)
{
int	i;

	if (gNumShards >= MAX_SHARDS)
		return(-1);

	for (i = 0; i < MAX_SHARDS; i++)
		if (gShards[i].isUsed == false)
			return(i);

	return(-1);
}


/******** SET SHARD XFORM MATRIX FROM SCALE, ROT, COORD **********/

static void UpdateShardTransformMatrix(ShardType* shard)
{
	OGLMatrix4x4	m1;
	OGLMatrix4x4	m2;

			/* SET SCALE MATRIX */

	OGLMatrix4x4_SetScale(&shard->matrix, shard->scale, shard->scale, shard->scale);

			/* NOW ROTATE IT */

	OGLMatrix4x4_SetRotate_XYZ(&m1, shard->rot.x, shard->rot.y, shard->rot.z);
	OGLMatrix4x4_Multiply(&shard->matrix, &m1, &m2);

			/* NOW TRANSLATE IT */

	OGLMatrix4x4_SetTranslate(&m1, shard->coord.x, shard->coord.y, shard->coord.z);
	OGLMatrix4x4_Multiply(&m2, &m1, &shard->matrix);
}


/****************** EXPLODE GEOMETRY ***********************/

void ExplodeGeometry(ObjNode *theNode, float boomForce, Byte particleMode, long particleDensity, float particleDecaySpeed)
{
MetaObjectPtr	theObject;

	gShardSrcObj	= theNode;
	gBoomForce 		= boomForce;
	gShardMode 		= particleMode;
	gShardDensity 	= particleDensity;
	gShardDecaySpeed = particleDecaySpeed;
	OGLMatrix4x4_SetIdentity(&gWorkMatrix);				// init to identity matrix


		/* SKELETON */

	if (theNode->Genre == SKELETON_GENRE)
	{
		short	numMeshes,i;
		Byte	buffNum = gGameViewInfoPtr->frameCount & 1;

		numMeshes = theNode->Skeleton->skeletonDefinition->numDecomposedTriMeshes;

		for (i = 0; i < numMeshes; i++)
		{
			ExplodeVertexArray(&theNode->Skeleton->deformedMeshes[buffNum][i], theNode->Skeleton->overrideTexture[i]);		// explode each trimesh individually
		}
	}

		/* DISPLAY GROUP */

	else
	if (theNode->Genre == DISPLAY_GROUP_GENRE)
	{
		theObject = theNode->BaseGroup;
		ExplodeGeometry_Recurse(theObject);
	}


				/* SUBRECURSE CHAINS */

	if (theNode->ChainNode)
		ExplodeGeometry(theNode->ChainNode, boomForce, particleMode, particleDensity, particleDecaySpeed);

}


/****************** EXPLODE GEOMETRY - RECURSE ***********************/

static void ExplodeGeometry_Recurse(MetaObjectPtr obj)
{
MOGroupObject		*groupObj;
MOMatrixObject 		*matObj;
OGLMatrix4x4		*transform;
OGLMatrix4x4  		stashMatrix;
int					i,numChildren;

MetaObjectHeader	*objHead = obj;
MOVertexArrayData	*vaData;

			/* VERIFY COOKIE */

	if (objHead->cookie != MO_COOKIE)
		DoFatalAlert("ExplodeGeometry_Recurse: cookie is invalid!");


			/* HANDLE TYPE */

	switch(objHead->type)
	{
		case	MO_TYPE_GEOMETRY:
				switch ((int) objHead->subType)
				{
					case	MO_GEOMETRY_SUBTYPE_VERTEXARRAY:
							vaData = &((MOVertexArrayObject *)obj)->objectData;
							ExplodeVertexArray(vaData, nil);
							break;

					default:
							DoFatalAlert("ExplodeGeometry_Recurse: unknown sub-type!");
				}
				break;

		case	MO_TYPE_MATERIAL:
				break;

		case	MO_TYPE_GROUP:
		 		groupObj = obj;
		  		stashMatrix = gWorkMatrix;											// push matrix

				numChildren = groupObj->objectData.numObjectsInGroup;				// get # objects in group

		  		for (i = 0; i < numChildren; i++)									// scan all objects in group
	    			ExplodeGeometry_Recurse(groupObj->objectData.groupContents[i]);	// sub-recurse this object

		  		gWorkMatrix = stashMatrix;											// pop matrix
				break;

		case	MO_TYPE_MATRIX:
				matObj = obj;
				transform = &matObj->matrix;										// point to matrix
				OGLMatrix4x4_Multiply(transform, &gWorkMatrix, &gWorkMatrix);		// multiply it in
				break;
	}
}


/********************** EXPLODE VERTEX ARRAY *******************************/

static void ExplodeVertexArray(MOVertexArrayData *data, MOMaterialObject *overrideTexture)
{
OGLPoint3D			centerPt = {0,0,0};
uint32_t				ind[3];
OGLTextureCoord		*uvPtr;
long				i,t;
float				boomForce = gBoomForce;
OGLPoint3D			origin;

//	if (gShardMode & SHARD_MODE_FROMORIGIN)
	{
		origin.x = gShardSrcObj->Coord.x + (gShardSrcObj->LocalBBox.max.x + gShardSrcObj->LocalBBox.min.x) * .5f;		// set origin to center of object's bbox
		origin.y = gShardSrcObj->Coord.y + (gShardSrcObj->LocalBBox.max.y + gShardSrcObj->LocalBBox.min.y) * .5f;
		origin.z = gShardSrcObj->Coord.z + (gShardSrcObj->LocalBBox.max.z + gShardSrcObj->LocalBBox.min.z) * .5f;
	}

			/***************************/
			/* SCAN THRU ALL TRIANGLES */
			/***************************/

	for (t = 0; t < data->numTriangles; t += gShardDensity)				// scan thru all triangles
	{
				/* GET FREE PARTICLE INDEX */

		i = FindFreeShard();
		if (i == -1)														// see if all out
			break;


				/* DO POINTS */

		ind[0] = data->triangles[t].vertexIndices[0];						// get indecies of 3 points
		ind[1] = data->triangles[t].vertexIndices[1];
		ind[2] = data->triangles[t].vertexIndices[2];

		gShards[i].points[0] = data->points[ind[0]];						// get coords of 3 points
		gShards[i].points[1] = data->points[ind[1]];
		gShards[i].points[2] = data->points[ind[2]];

		OGLPoint3D_Transform(&gShards[i].points[0],&gWorkMatrix,&gShards[i].points[0]);							// transform points
		OGLPoint3D_Transform(&gShards[i].points[1],&gWorkMatrix,&gShards[i].points[1]);
		OGLPoint3D_Transform(&gShards[i].points[2],&gWorkMatrix,&gShards[i].points[2]);

		centerPt.x = (gShards[i].points[0].x + gShards[i].points[1].x + gShards[i].points[2].x) * 0.3333f;		// calc center of polygon
		centerPt.y = (gShards[i].points[0].y + gShards[i].points[1].y + gShards[i].points[2].y) * 0.3333f;
		centerPt.z = (gShards[i].points[0].z + gShards[i].points[1].z + gShards[i].points[2].z) * 0.3333f;

		gShards[i].points[0].x -= centerPt.x;								// offset coords to be around center
		gShards[i].points[0].y -= centerPt.y;
		gShards[i].points[0].z -= centerPt.z;
		gShards[i].points[1].x -= centerPt.x;
		gShards[i].points[1].y -= centerPt.y;
		gShards[i].points[1].z -= centerPt.z;
		gShards[i].points[2].x -= centerPt.x;
		gShards[i].points[2].y -= centerPt.y;
		gShards[i].points[2].z -= centerPt.z;


				/* DO VERTEX UV'S */

		uvPtr = data->uvs[0];
		if (uvPtr)																// see if also has UV (texture layer 0 only!)
		{
			gShards[i].uvs[0] = uvPtr[ind[0]];									// get vertex u/v's
			gShards[i].uvs[1] = uvPtr[ind[1]];
			gShards[i].uvs[2] = uvPtr[ind[2]];
		}

				/* DO MATERIAL INFO */

		if (overrideTexture)
			gShards[i].material = overrideTexture;
		else
		{
			if (data->numMaterials > 0)
				gShards[i].material = data->materials[0];							// keep material ptr
			else
				gShards[i].material = nil;
		}

		gShards[i].colorFilter = gShardSrcObj->ColorFilter;						// keep color
		gShards[i].glow = gShardSrcObj->StatusBits & STATUS_BIT_GLOW;

			/*********************/
			/* SET PHYSICS STUFF */
			/*********************/

		gShards[i].coord 	= centerPt;
		gShards[i].rot.x 	= gShards[i].rot.y = gShards[i].rot.z = 0;
		gShards[i].scale 	= 1.0;

		if (gShardMode & SHARD_MODE_FROMORIGIN)								// see if random deltas or from origin
		{
			OGLVector3D	v;

			v.x = centerPt.x - origin.x;									// calc vector from object's origin
			v.y = centerPt.y - origin.y;
			v.z = centerPt.z - origin.z;
			FastNormalizeVector(v.x, v.y, v.z, &v);

			gShards[i].coordDelta.x = v.x * boomForce;
			gShards[i].coordDelta.y = v.y * boomForce;
			gShards[i].coordDelta.z = v.z * boomForce;
		}
		else
		{
			gShards[i].coordDelta.x = RandomFloat2() * boomForce;
			gShards[i].coordDelta.y = RandomFloat2() * boomForce;
			gShards[i].coordDelta.z = RandomFloat2() * boomForce;
		}

		if (gShardMode & SHARD_MODE_UPTHRUST)
			gShards[i].coordDelta.y += 1.5f * gBoomForce;

		gShards[i].rotDelta.x 	= RandomFloat2() * (boomForce * .008f);			// random rotation deltas
		gShards[i].rotDelta.y 	= RandomFloat2() * (boomForce * .008f);
		gShards[i].rotDelta.z	= RandomFloat2() * (boomForce * .008f);

		gShards[i].decaySpeed 	= gShardDecaySpeed;
		gShards[i].mode 		= gShardMode;

				/* SET INITIAL XFORM MATRIX */

		UpdateShardTransformMatrix(&gShards[i]);

				/* SET VALID & INC COUNTER */

		gShards[i].isUsed = true;
		gNumShards++;
	}
}


/************************** MOVE SHARDS ****************************/

static void MoveShards(ObjNode *theNode)
{
#pragma unused (theNode)
float	ty,y,fps,x,z;
long	i;

	if (gNumShards == 0)												// quick check if any particles at all
		return;

	GAME_ASSERT(gNumShards > 0);

	fps = gFramesPerSecondFrac;

	for (i=0; i < MAX_SHARDS; i++)
	{
		if (!gShards[i].isUsed)
		{
			continue;
		}

				/* ROTATE IT */

		gShards[i].rot.x += gShards[i].rotDelta.x * fps;
		gShards[i].rot.y += gShards[i].rotDelta.y * fps;
		gShards[i].rot.z += gShards[i].rotDelta.z * fps;

					/* MOVE IT */

		if (gShards[i].mode & SHARD_MODE_HEAVYGRAVITY)
			gShards[i].coordDelta.y -= fps * 1000.0f;		// gravity
		else
			gShards[i].coordDelta.y -= fps * 300.0f;		// gravity

		x = (gShards[i].coord.x += gShards[i].coordDelta.x * fps);
		y = (gShards[i].coord.y += gShards[i].coordDelta.y * fps);
		z = (gShards[i].coord.z += gShards[i].coordDelta.z * fps);


					/* SEE IF BOUNCE */

		ty = GetTerrainY(x,z);								// get terrain height here
		if (y <= ty)
		{
			if (gShards[i].mode & SHARD_MODE_BOUNCE)
			{
				gShards[i].coord.y  = ty;
				gShards[i].coordDelta.y *= -0.5f;
				gShards[i].coordDelta.x *= 0.9f;
				gShards[i].coordDelta.z *= 0.9f;
			}
			else
				goto del;
		}

					/* SCALE IT */

		gShards[i].scale -= gShards[i].decaySpeed * fps;
		if (gShards[i].scale <= 0.0f)
		{
				/* DEACTIVATE THIS PARTICLE */
del:
			gShards[i].isUsed = false;
			gNumShards--;
			continue;
		}

			/***************************/
			/* UPDATE TRANSFORM MATRIX */
			/***************************/

		UpdateShardTransformMatrix(&gShards[i]);
	}
}


/************************* SHARD LOOKS LIKE SHARD ****************************/
//
// Returns true if both shards can be drawn in the same draw call.
//

static Boolean ShardLooksLikeShard(const ShardType* a, const ShardType* b)
{
	return a->material == b->material
		&& a->glow == b->glow
		&& a->colorFilter.r == b->colorFilter.r
		&& a->colorFilter.g == b->colorFilter.g
		&& a->colorFilter.b == b->colorFilter.b
		&& a->colorFilter.a == b->colorFilter.a;
}


/************************* DRAW SHARDS ****************************/
//
// Each shard is a single triangle with its own transform. Instead of issuing one draw call per shard,
// shards that look alike are pre-transformed into a shared vertex buffer and drawn together.
// This matters a lot in the browser, where each draw call is expensive.
//

static void DrawShards(ObjNode *theNode)
{
#pragma unused (theNode)

static OGLPoint3D		batchPoints[MAX_SHARDS * 3];
static OGLVector3D		batchNormals[MAX_SHARDS * 3];
static OGLTextureCoord	batchUVs[MAX_SHARDS * 3];
static Boolean			shardDrawn[MAX_SHARDS];

	if (gNumShards == 0)												// quick check if any particles at all
		return;

	GAME_ASSERT(gNumShards > 0);

			/* SET STATE */

	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);

	// The shards don't have normals of their own, so they're lit with the current normal,
	// which GL transforms by each shard's matrix (inverse transpose of scale*rotation = rotation/scale)
	OGLVector3D currentNormal;
	glGetFloatv(GL_CURRENT_NORMAL, (GLfloat*) &currentNormal);

	for (int i = 0; i < MAX_SHARDS; i++)
		shardDrawn[i] = !gShards[i].isUsed;

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_NORMAL_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
	glVertexPointer(3, GL_FLOAT, 0, batchPoints);
	glNormalPointer(GL_FLOAT, 0, batchNormals);
	glTexCoordPointer(2, GL_FLOAT, 0, batchUVs);

	for (int first = 0; first < MAX_SHARDS; first++)
	{
		if (shardDrawn[first])
			continue;

		const ShardType* look = &gShards[first];

				/* SUBMIT MATERIAL */

		gGlobalColorFilter.r = look->colorFilter.r;
		gGlobalColorFilter.g = look->colorFilter.g;
		gGlobalColorFilter.b = look->colorFilter.b;
		gGlobalTransparency = look->colorFilter.a;

		if (look->glow)
			OGL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
		else
		    OGL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		if (look->material)
			MO_DrawMaterial(look->material);

				/* TRANSFORM ALL SHARDS THAT LOOK THE SAME INTO THE BATCH */

		int numPoints = 0;

		for (int i = first; i < MAX_SHARDS; i++)
		{
			if (shardDrawn[i] || !ShardLooksLikeShard(&gShards[i], look))
				continue;

			const ShardType* shard = &gShards[i];
			shardDrawn[i] = true;

			OGLVector3D normal;
			OGLVector3D_Transform(&currentNormal, &shard->matrix, &normal);		// = scale * rotated normal
			float normalScale = 1.0f / (shard->scale * shard->scale);
			normal.x *= normalScale;
			normal.y *= normalScale;
			normal.z *= normalScale;

			for (int v = 0; v < 3; v++)
			{
				OGLPoint3D_Transform(&shard->points[v], &shard->matrix, &batchPoints[numPoints]);
				batchNormals[numPoints] = normal;
				batchUVs[numPoints] = shard->uvs[v];
				numPoints++;
			}
		}

				/* DRAW THE BATCH */

		glDrawArrays(GL_TRIANGLES, 0, numPoints);
	}

		/* CLEANUP */

	glDisableClientState(GL_NORMAL_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glNormal3f(currentNormal.x, currentNormal.y, currentNormal.z);

	gGlobalColorFilter.r =
	gGlobalColorFilter.g =
	gGlobalColorFilter.b =
	gGlobalTransparency = 1;
    OGL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   	glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_FALSE);

}









