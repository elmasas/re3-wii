#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "src/rwbase.h"
#include "src/rwerror.h"
#include "src/rwplg.h"
#include "src/rwrender.h"
#include "src/rwengine.h"
#include "src/rwpipeline.h"
#include "src/rwobjects.h"
#include "src/rwanim.h"
#include "src/rwplugins.h"
#include "src/rwuserdata.h"
#include "src/rwcharset.h"
#include "src/ps2/rwps2.h"
#include "src/ps2/rwps2plg.h"
#include "src/d3d/rwxbox.h"
#include "src/d3d/rwd3d.h"
#include "src/d3d/rwd3d8.h"
#include "src/d3d/rwd3d9.h"
#include "src/gl/rwwdgl.h"
#include "src/gl/rwgl3.h"
#include "src/gl/rwgl3shader.h"
#include "src/gl/rwgl3plg.h"
#ifdef __WIIU__
#include "src/gx2/rwgx2.h"
#include "src/gx2/rwgx2shader.h"
#include "src/gx2/rwgx2plg.h"
#endif

#ifdef __WII__
#include "src/gx/rwgx.h"
#endif
