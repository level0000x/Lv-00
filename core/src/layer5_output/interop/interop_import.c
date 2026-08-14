/**
 * @file interop_import.c
 * @brief 导入（GeoGebra/SVG）
 *
 * @details 拆分子模块（Lv-00 v3.3.0+）：本文件保留文件头与 include 聚合入口，
 *          实现按格式拆分至：
 *          - interop_import_ggb_zip.c：GeoGebra ZIP 解析 + Deflate 解压
 *          - interop_import_ggb_xml.c：GGB XML 解析 + 元素导入 + interop_import_geogebra
 *          - interop_import_json.c：GeoJSON 导入 + interop_import_geojson
 *          - interop_import_svg.c：SVG 导入 + interop_import_svg
 *          跨子模块共享内部函数声明见 interop_import_internal.h。
 * @author Lv-00 Project
 * @version 3.3.0
 */

#include "lv/lv_platform.h"
#include <errno.h>
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "lv/lv_file.h"

#include "lv/constraint_graph.h"
#include "lv/engine.h"
#include "lv/interop.h"
#include "lv/lv_json.h"
#include "lv/lv_parse_utils.h"
#include "lv/lv_str_utils.h"
#include "lv/geo_utils.h"
#include "lv/lv_numeric.h"


#include "lv/debug.h"
#include "lv/lv_internal.h"
#include "lv/lv_utils.h"

#include "interop_import_internal.h"
