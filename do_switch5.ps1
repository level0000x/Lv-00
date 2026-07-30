$fp = "core\src\layer1_parser\formula_converter.c"
$content = [System.IO.File]::ReadAllText($fp, [System.Text.Encoding]::UTF8)

$funcs = @'

/* 函数指针表：graph_to_formula 节点格式化 */
typedef void (*GtfNodeFunc)(const GeomNode *node, const char *name,
                             GraphToFormulaResult *result,
                             size_t *ll, size_t *pl, size_t *dl,
                             size_t ls, size_t ps, size_t ds);

static void gtf_point(const GeomNode *node, const char *name,
                       GraphToFormulaResult *r,
                       size_t *ll, size_t *pl, size_t *dl,
                       size_t ls, size_t ps, size_t ds) {
    double x=0,y=0;
    if(node->symbolic_coords&&node->coord_count>=2){
        x=symbolic_coord_to_double(node->symbolic_coords[0]);
        y=symbolic_coord_to_double(node->symbolic_coords[1]);
    }
    char b[512];
    int n=snprintf(b,sizeof(b),"%s = \left(%.2f, %.2f\right)\\\\n",name,x,y);
    if(n>0&&*ll+(size_t)n<ls){memcpy(r->latex_output+*ll,b,(size_t)n);*ll+=(size_t)n;r->latex_output[*ll]='\0';}
    n=snprintf(b,sizeof(b),"%s = Point(%.2f, %.2f)\n",name,x,y);
    if(n>0&&*pl+(size_t)n<ps){memcpy(r->python_output+*pl,b,(size_t)n);*pl+=(size_t)n;r->python_output[*pl]='\0';}
    n=snprintf(b,sizeof(b),"point %s(%.2f, %.2f); ",name,x,y);
    if(n>0&&*dl+(size_t)n<ds){memcpy(r->dsl_output+*dl,b,(size_t)n);*dl+=(size_t)n;r->dsl_output[*dl]='\0';}
}

static void gtf_seg(const GeomNode *node, const char *name,
                     GraphToFormulaResult *r,
                     size_t *ll, size_t *pl, size_t *dl,
                     size_t ls, size_t ps, size_t ds) {
    char b[512];
    int n=snprintf(b,sizeof(b),"\overline{%s}\\\\n",name);
    if(n>0&&*ll+(size_t)n<ls){memcpy(r->latex_output+*ll,b,(size_t)n);*ll+=(size_t)n;r->latex_output[*ll]='\0';}
    n=snprintf(b,sizeof(b),"%s = Segment()\n",name);
    if(n>0&&*pl+(size_t)n<ps){memcpy(r->python_output+*pl,b,(size_t)n);*pl+=(size_t)n;r->python_output[*pl]='\0';}
    n=snprintf(b,sizeof(b),"segment %s(); ",name);
    if(n>0&&*dl+(size_t)n<ds){memcpy(r->dsl_output+*dl,b,(size_t)n);*dl+=(size_t)n;r->dsl_output[*dl]='\0';}
}

'@

$idx = $content.IndexOf("switch (node->type) {")
Write-Host ("Switch at: " + $idx)
