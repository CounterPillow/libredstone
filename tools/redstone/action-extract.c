/*
 * This program is part of libredstone.
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "redstone.h"
#include <stdio.h>

#include "options.h"
#include "formats.h"

int rs_tool_extract(RSToolOptions* opts)
{
    if (!(opts->formatter->dump))
    {
        opts->error("format `%s' does not support extraction\n", opts->formatter->name);
        return 1;
    }
    opts->formatter->dump(opts, opts->source.nbt, stdout);
    return 0;
}

