#include "sysconfig.h"
#include "sysdeps.h"
#include "memory.h"
#include "uade_logging.h"

#include "uadectl.h"
#include <uade/uadeipc.h>

#include <ctype.h>
#include <string.h>

#include <zakalwe/array.h>
#include <zakalwe/map.h>
#include <zakalwe/string.h>

Z_MAP_PROTOTYPE(str_key_map, char *, int, strcmp)

static struct str_key_map *m_logging;

static void free_str(char *s)
{
	free(s);
}

/*
 * TODO: Put _escape_python_str to a common module between uadecore and the
 * frontend.
 */
static void _escape_python_str(struct z_string *zs, const char *s)
{
	char buf[5];
	for (size_t i = 0; i < strlen(s); i++) {
		const int c = s[i];
		if (c > 0 && c <= 127 && isprint(c) &&
		    c != '\'' && c != '\\') {
			buf[0] = c;
			buf[1] = 0;
		} else {
			z_assert(
				snprintf(buf, sizeof(buf), "\\x%x",
					 (unsigned char) c) <= sizeof(buf));
		}
		z_assert(z_string_cat_c_str(zs, buf));
	}
}
static void _write_key_int(struct z_string *zs, const char *key, const int x)
{
	char buf[32];
	snprintf(buf, sizeof(buf), "%d", x);
	z_assert(z_string_cat_c_str(zs, "'"));
	_escape_python_str(zs, key);
	z_assert(z_string_cat_c_str(zs, "': "));
	_escape_python_str(zs, buf);
	z_assert(z_string_cat_c_str(zs, ", "));
}

void uade_logging_str(const char *s)
{
	if (m_logging == NULL) {
		m_logging = str_key_map_create();
		z_assert(m_logging != NULL);
		str_key_map_cleanup_keys(m_logging, free_str);
	}

	char *key = strdup(s);
	z_assert(key != NULL);

	int freq = str_key_map_get_default(m_logging, key, 0);
	z_assert(str_key_map_set(m_logging, key, freq + 1));
}

void uade_logging_flush(void)
{
	if (m_logging == NULL)
		return;

	struct str_key_map_iter iter;
	/* Write out logs in Python AST format */
	struct z_string *ast = z_string_create();
	z_assert(ast != NULL);
	z_assert(z_string_cat_c_str(ast, "{"));

	z_map_for_each(str_key_map, m_logging, &iter) {
		_write_key_int(ast, iter.key, iter.value);
	}

	z_assert(z_string_cat_c_str(ast, "}"));
	if (uade_send_string(UADE_COMMAND_SEND_UADECORE_LOGS, ast->s,
			     &uadecore_ipc)) {
		fprintf(stderr, "\nuadecore: Unable to send logs: %s\n",
			ast->s);
	}
	z_string_free(ast);

	str_key_map_free(m_logging);
	m_logging = NULL;
}
