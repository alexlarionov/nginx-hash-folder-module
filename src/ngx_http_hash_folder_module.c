#include <ngx_config.h>
#include <ngx_core.h>
#include <ngx_http.h>


typedef struct {
    ngx_http_complex_value_t    value;
    ngx_http_complex_value_t    prefix;
} ngx_http_hash_folder_ctx_t;

static ngx_str_t main_str = ngx_string("!main");

static char *ngx_conf_hash_folder_block(ngx_conf_t *cf, ngx_command_t *cmd,
    void *conf);

static ngx_command_t  ngx_http_hash_folder_commands[] = {

    { ngx_string("hash_folder"),
      NGX_HTTP_MAIN_CONF|NGX_HTTP_SRV_CONF|NGX_HTTP_LOC_CONF|NGX_HTTP_LIF_CONF|NGX_CONF_TAKE3,
      ngx_conf_hash_folder_block,
      NGX_HTTP_LOC_CONF_OFFSET,
      0,
      NULL },

      ngx_null_command
};


static ngx_http_module_t  ngx_http_hash_folder_module_ctx = {
    NULL,                                  /* preconfiguration */
    NULL,                                  /* postconfiguration */

    NULL,                                  /* create main configuration */
    NULL,                                  /* init main configuration */

    NULL,                                  /* create server configuration */
    NULL,                                  /* merge server configuration */

    NULL,                                  /* create location configuration */
    NULL                                   /* merge location configuration */
};


ngx_module_t  ngx_http_hash_folder_module = {
    NGX_MODULE_V1,
    &ngx_http_hash_folder_module_ctx,    /* module context */
    ngx_http_hash_folder_commands,       /* module directives */
    NGX_HTTP_MODULE,                       /* module type */
    NULL,                                  /* init master */
    NULL,                                  /* init module */
    NULL,                                  /* init process */
    NULL,                                  /* init thread */
    NULL,                                  /* exit thread */
    NULL,                                  /* exit process */
    NULL,                                  /* exit master */
    NGX_MODULE_V1_PADDING
};


static ngx_int_t
ngx_http_hash_folder_variable(ngx_http_request_t *r,
    ngx_http_variable_value_t *v, uintptr_t data)
{
    ngx_http_hash_folder_ctx_t *ctx = (ngx_http_hash_folder_ctx_t *) data;
    u_char            *p;
    uint32_t                        hash;
    ngx_str_t                       val;
    ngx_str_t                       prefix;

    *v = ngx_http_variable_null_value;

    if (ngx_http_complex_value(r, &ctx->value, &val) != NGX_OK) {
        return NGX_ERROR;
    }
    if (ngx_http_complex_value(r, &ctx->prefix, &prefix) != NGX_OK) {
        return NGX_ERROR;
    }

    if (val.len == 0) {
        p = ngx_pnalloc(r->pool, 1 + prefix.len + main_str.len);
        if (p == NULL) {
            return NGX_ERROR;
        }
        v->len = ngx_sprintf(p, "%V/%V", &prefix, &main_str) - p;
    } else {
        p = ngx_pnalloc(r->pool, 9 + prefix.len + val.len);
        if (p == NULL) {
            return NGX_ERROR;
        }
        hash = ngx_murmur_hash2(val.data, val.len);
        v->len = ngx_sprintf(p, "%V/%ui/%ui/%V", &prefix, hash & 0xFF, (hash >> 8) & 0xFF, &val) - p;
    }
    v->valid = 1;
    v->no_cacheable = 0;
    v->not_found = 0;
    v->data = p;

    return NGX_OK;
}


static char *
ngx_conf_hash_folder_block(ngx_conf_t *cf, ngx_command_t *cmd, void *conf)
{
    ngx_str_t                           *value, name;
    ngx_http_variable_t                 *var;
    ngx_http_hash_folder_ctx_t          *ctx;
    ngx_http_compile_complex_value_t     ccv;

    ctx = ngx_pcalloc(cf->pool, sizeof(ngx_http_hash_folder_ctx_t));
    if (ctx == NULL) {
        return NGX_CONF_ERROR;
    }

    value = cf->args->elts;

    ngx_memzero(&ccv, sizeof(ngx_http_compile_complex_value_t));

    ccv.cf = cf;
    ccv.value = &value[1];
    ccv.complex_value = &ctx->value;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    ccv.value = &value[3];
    ccv.complex_value = &ctx->prefix;

    if (ngx_http_compile_complex_value(&ccv) != NGX_OK) {
        return NGX_CONF_ERROR;
    }

    name = value[2];

    if (name.data[0] != '$') {
        ngx_conf_log_error(NGX_LOG_EMERG, cf, 0,
                           "invalid variable name \"%V\"", &name);
        return NGX_CONF_ERROR;
    }

    name.len--;
    name.data++;

    var = ngx_http_add_variable(cf, &name, NGX_HTTP_VAR_CHANGEABLE);
    if (var == NULL) {
        return NGX_CONF_ERROR;
    }

    var->get_handler = ngx_http_hash_folder_variable;
    var->data = (uintptr_t) ctx;

    return NGX_CONF_OK;
}
