/*
 * This file is part of libredstone, and is distributed under the GNU LGPL.
 * See redstone.h for details.
 */

#include "tag.h"

#include "error.h"
#include "memory.h"
#include "list.h"

/* used in the compound tag RSList */
typedef struct
{
    char* key;
    RSTag* value;
} RSTagCompoundNode;

struct _RSTag
{
    uint32_t refcount;
    RSTagType type;
    
    union
    {
        int8_t int_byte;
        int16_t int_short;
        int32_t int_int;
        int64_t int_long;
        
        float float_float;
        double float_double;
        
        struct
        {
            uint32_t size;
            uint8_t* data;
        } byte_array;
        char* string;
        struct
        {
            RSTagType type;
            RSList* items;
        } list;
        RSList* compound;
    };
};

RSTag* rs_tag_new0(RSTagType type)
{
    rs_return_val_if_fail(type != RS_TAG_END, NULL);
    
    RSTag* self = rs_new0(RSTag, 1);
    self->refcount = 0; /* floating reference */
    self->type = type;
    return self;
}

RSTagType rs_tag_get_type(RSTag* self)
{
    rs_return_val_if_fail(self, RS_TAG_END);
    return self->type;
}

RSTag* rs_tag_new(RSTagType type, ...)
{
    va_list ap;
    RSTag* self;
    
    va_start(ap, type);
    self = rs_tag_newv(type, ap);
    va_end(ap);
    
    return self;
}

RSTag* rs_tag_newv(RSTagType type, va_list ap)
{
    RSTag* self = rs_tag_new0(type);
    
    char* key;
    RSTag* tag;
    bool first = true;
    uint32_t len;
    void* data;
    
    switch (type)
    {
    case RS_TAG_BYTE:
    case RS_TAG_SHORT:
    case RS_TAG_INT:
    case RS_TAG_LONG:
        rs_tag_set_integer(self, va_arg(ap, int));
        break;
    case RS_TAG_FLOAT:
    case RS_TAG_DOUBLE:
        rs_tag_set_float(self, va_arg(ap, double));
        break;
    case RS_TAG_BYTE_ARRAY:
        len = va_arg(ap, int);
        data = va_arg(ap, void*);
        rs_tag_set_byte_array(self, len, data);
        break;
    case RS_TAG_STRING:
        rs_tag_set_string(self, va_arg(ap, char*));
        break;
    case RS_TAG_LIST:
        while (tag = va_arg(ap, RSTag*))
        {
            if (first)
            {
                rs_tag_list_set_type(self, rs_tag_get_type(tag));
                first = false;
            }
            rs_tag_list_insert(self, 0, tag);
        }
        rs_tag_list_reverse(self);
        break;
    case RS_TAG_COMPOUND:
        while (key = va_arg(ap, char*))
        {
            tag = va_arg(ap, RSTag*);
            rs_return_val_if_fail(tag, NULL);
            
            rs_tag_compound_set(self, key, tag);
        }
        break;
    };
    
    return self;
}

/* internal free, used by unref */
static void _rs_tag_free(RSTag* self)
{
    rs_return_if_fail(self);
    rs_return_if_fail(self->refcount == 0);
    
    RSList* cell;
    
    switch (self->type)
    {
    case RS_TAG_BYTE_ARRAY:
        rs_free(self->byte_array.data);
        break;
    case RS_TAG_STRING:
        rs_free(self->string);
        break;
    case RS_TAG_LIST:
        rs_list_foreach(self->list.items, (RSListFunction)rs_tag_unref);
        rs_list_free(self->list.items);
        break;
    case RS_TAG_COMPOUND:
        cell = self->compound;
        for (; cell != NULL; cell = cell->next)
        {
            RSTagCompoundNode* node = (RSTagCompoundNode*)(cell->data);
            rs_assert(node);
            rs_assert(node->key);
            rs_assert(node->value);
            
            rs_free(node->key);
            rs_tag_unref(node->value);
            rs_free(node);
        }
        
        rs_list_free(self->compound);
        break;
    };
    
    rs_free(self);
}

void rs_tag_ref(RSTag* self)
{
    rs_return_if_fail(self);
    self->refcount++;
}

void rs_tag_unref(RSTag* self)
{
    rs_return_if_fail(self);

    if (self->refcount > 0)
        self->refcount--;
    if (self->refcount == 0)
        _rs_tag_free(self);
}

RSTag* rs_tag_find(RSTag* self, const char* name)
{
    rs_return_val_if_fail(self && self->type != RS_TAG_END, NULL);
    rs_return_val_if_fail(name, NULL);
    
    RSTag* subtag;
    RSTagIterator it;
    
    switch (rs_tag_get_type(self))
    {
    case RS_TAG_END:
        rs_assert(false);
    case RS_TAG_BYTE:
    case RS_TAG_SHORT:
    case RS_TAG_INT:
    case RS_TAG_LONG:
    case RS_TAG_FLOAT:
    case RS_TAG_DOUBLE:
    case RS_TAG_BYTE_ARRAY:
    case RS_TAG_STRING:
        /* leaf nodes, no children */
        return NULL;
    case RS_TAG_LIST:
        /* search each element */
        rs_tag_list_iterator_init(self, &it);
        while (rs_tag_list_iterator_next(&it, &subtag))
        {
            RSTag* found = rs_tag_find(subtag, name);
            if (found)
                return found;
        }
        
        return NULL;
    case RS_TAG_COMPOUND:
        /* first, check to see if it's in this compound */
        subtag = rs_tag_compound_get(self, name);
        if (subtag)
            return subtag;

        /* search each element */
        rs_tag_compound_iterator_init(self, &it);
        while (rs_tag_compound_iterator_next(&it, NULL, &subtag))
        {
            RSTag* found = rs_tag_find(subtag, name);
            if (found)
                return found;
        }
        
        return NULL;
    default:
        rs_return_val_if_reached(NULL);
    };
    rs_return_val_if_reached(NULL);
}

void rs_tag_print(RSTag* self, FILE* dest)
{
    rs_return_if_fail(self && self->type != RS_TAG_END);
    rs_return_if_fail(dest);
    
    uint32_t length, i;
    RSTagIterator it;
    RSTag* subtag;
    const char* subname;

    switch (rs_tag_get_type(self))
    {
    case RS_TAG_END:
        rs_assert(false);
    case RS_TAG_BYTE:
    case RS_TAG_SHORT:
    case RS_TAG_INT:
    case RS_TAG_LONG:
        fprintf(dest, "%Li", (long long)rs_tag_get_integer(self));
        break;
    case RS_TAG_FLOAT:
    case RS_TAG_DOUBLE:
        fprintf(dest, "%f", rs_tag_get_float(self));
        break;
    case RS_TAG_BYTE_ARRAY:
        length = rs_tag_get_byte_array_length(self);
        if (length != fwrite(rs_tag_get_byte_array(self), 1, length, dest))
            rs_critical("could not write entire byte array");
        break;
    case RS_TAG_STRING:
        fprintf(dest, "%s", rs_tag_get_string(self));
        break;
    case RS_TAG_LIST:
        length = rs_tag_list_get_length(self);        
        fprintf(dest, "[");
        
        rs_tag_list_iterator_init(self, &it);
        i = 0;
        while (rs_tag_list_iterator_next(&it, &subtag))
        {
            i++;
            if (subtag->type == RS_TAG_STRING)
                fprintf(dest, "\"");
            rs_tag_print(subtag, dest);
            if (subtag->type == RS_TAG_STRING)
                fprintf(dest, "\"");
            if (i != length)
                fprintf(dest, ", ");
        }
        
        fprintf(dest, "]");
        break;
    case RS_TAG_COMPOUND:
        length = rs_tag_compound_get_length(self);
        fprintf(dest, "{");
        
        rs_tag_compound_iterator_init(self, &it);
        i = 0;
        while (rs_tag_compound_iterator_next(&it, &subname, &subtag))
        {
            printf("\"%s\": ", subname);
            
            i++;
            if (subtag->type == RS_TAG_STRING)
                fprintf(dest, "\"");
            rs_tag_print(subtag, dest);
            if (subtag->type == RS_TAG_STRING)
                fprintf(dest, "\"");
            if (i != length)
                fprintf(dest, ", ");
        }
        
        fprintf(dest, "}");
        break;
    default:
        rs_return_if_reached();
    };
}

/* helper for rs_tag_pretty_print */
static inline const char* rs_tag_get_type_name(RSTagType type)
{
    switch (type)
    {
    case RS_TAG_END: return "TAG_End";
    case RS_TAG_BYTE: return "TAG_Byte";
    case RS_TAG_SHORT: return "TAG_Short";
    case RS_TAG_INT: return "TAG_Int";
    case RS_TAG_LONG: return "TAG_Long";
    case RS_TAG_FLOAT: return "TAG_Float";
    case RS_TAG_DOUBLE: return "TAG_Double";
    case RS_TAG_BYTE_ARRAY: return "TAG_Byte_Array";
    case RS_TAG_STRING: return "TAG_String";
    case RS_TAG_LIST: return "TAG_List";
    case RS_TAG_COMPOUND: return "TAG_Compound";
    }
    
    return "TAG_Unknown";
}

/* helper for rs_tag_pretty_print */
static inline void rs_tag_print_indent(FILE* dest, unsigned int indent)
{
    while (indent > 0)
    {
        fprintf(dest, "    ");
        indent--;
    }
}

/* inner part of rs_tag_pretty_print */
static void rs_tag_print_inner(FILE* dest, RSTag* tag, const char* name, unsigned int indent)
{
    rs_assert(tag);
    rs_assert(dest);
    rs_return_if_fail(tag->type != RS_TAG_END);
    
    rs_tag_print_indent(dest, indent);
    fprintf(dest, "%s", rs_tag_get_type_name(rs_tag_get_type(tag)));
    if (name)
        fprintf(dest, "(\"%s\")", name);
    fprintf(dest, ": ");
    
    RSTagIterator it;
    const char* subname;
    RSTag* subtag;
    
    switch (rs_tag_get_type(tag))
    {
    case RS_TAG_END:
        rs_assert(false);
    case RS_TAG_BYTE:
    case RS_TAG_SHORT:
    case RS_TAG_INT:
    case RS_TAG_LONG:
        fprintf(dest, "%Li\n", (long long)rs_tag_get_integer(tag));
        break;
    case RS_TAG_FLOAT:
    case RS_TAG_DOUBLE:
        fprintf(dest, "%f\n", rs_tag_get_float(tag));
        break;
    case RS_TAG_BYTE_ARRAY:
        fprintf(dest, "%u bytes\n", rs_tag_get_byte_array_length(tag));
        break;
    case RS_TAG_STRING:
        fprintf(dest, "%s\n", rs_tag_get_string(tag));
        break;
    case RS_TAG_LIST:
        fprintf(dest, "%u entries of type %s\n", rs_tag_list_get_length(tag), rs_tag_get_type_name(rs_tag_list_get_type(tag)));
        rs_tag_print_indent(dest, indent);
        fprintf(dest, "{\n");
        
        rs_tag_list_iterator_init(tag, &it);
        while (rs_tag_list_iterator_next(&it, &subtag))
        {
            rs_tag_print_inner(dest, subtag, NULL, indent + 1);
        }
        
        rs_tag_print_indent(dest, indent);
        fprintf(dest, "}\n");
        break;
    case RS_TAG_COMPOUND:
        fprintf(dest, "%i entries\n", rs_tag_compound_get_length(tag));
        rs_tag_print_indent(dest, indent);
        fprintf(dest, "{\n");
        
        rs_tag_compound_iterator_init(tag, &it);
        while (rs_tag_compound_iterator_next(&it, &subname, &subtag))
        {
            rs_tag_print_inner(dest, subtag, subname, indent + 1);
        }
        
        rs_tag_print_indent(dest, indent);
        fprintf(dest, "}\n");
        break;
    default:
        rs_return_if_reached();
    };
}

void rs_tag_pretty_print(RSTag* self, FILE* dest)
{
    rs_return_if_fail(self);
    rs_return_if_fail(dest);
    
    rs_tag_print_inner(dest, self, NULL, 0);
}

/* for integers */
int64_t rs_tag_get_integer(RSTag* self)
{
    rs_return_val_if_fail(self, 0);
    switch (self->type)
    {
    case RS_TAG_BYTE:
        return self->int_byte;
    case RS_TAG_SHORT:
        return self->int_short;
    case RS_TAG_INT:
        return self->int_int;
    case RS_TAG_LONG:
        return self->int_long;
    };
    
    rs_critical("rs_tag_get_integer called on non-integer type");
    return 0;
}

void rs_tag_set_integer(RSTag* self, int64_t val)
{
    rs_return_if_fail(self);
    switch (self->type)
    {
    case RS_TAG_BYTE:
        self->int_byte = val;
        return;
    case RS_TAG_SHORT:
        self->int_short = val;
        return;
    case RS_TAG_INT:
        self->int_int = val;
        return;
    case RS_TAG_LONG:
        self->int_long = val;
        return;
    };
    
    rs_critical("rs_tag_set_integer called on non-integer type");
}

/* for floats/doubles -- conversion is automatic */
double rs_tag_get_float(RSTag* self)
{
    rs_return_val_if_fail(self, 0.0);
    rs_return_val_if_fail(self->type == RS_TAG_FLOAT || self->type == RS_TAG_DOUBLE, 0.0);
    
    if (self->type == RS_TAG_FLOAT)
        return self->float_float;
    return self->float_double;
}

void rs_tag_set_float(RSTag* self, double val)
{
    rs_return_if_fail(self);
    rs_return_if_fail(self->type == RS_TAG_FLOAT || self->type == RS_TAG_DOUBLE);
    
    if (self->type == RS_TAG_FLOAT)
    {
        self->float_float = val;
    } else {
        self->float_double = val;
    }
}

/* for byte arrays */
uint8_t* rs_tag_get_byte_array(RSTag* self)
{
    rs_return_val_if_fail(self && self->type == RS_TAG_BYTE_ARRAY, NULL);
    return self->byte_array.data;
}

uint32_t rs_tag_get_byte_array_length(RSTag* self)
{
    rs_return_val_if_fail(self && self->type == RS_TAG_BYTE_ARRAY, 0);
    return self->byte_array.size;
}

void rs_tag_set_byte_array(RSTag* self, uint32_t len, uint8_t* data)
{
    rs_return_if_fail(self && self->type == RS_TAG_BYTE_ARRAY);
    uint8_t* olddata = self->byte_array.data;
    self->byte_array.data = rs_memdup(data, len);
    self->byte_array.size = len;
    if (olddata)
        rs_free(olddata);
}

/* for strings */
const char* rs_tag_get_string(RSTag* self)
{
    rs_return_val_if_fail(self && self->type == RS_TAG_STRING, NULL);
    return self->string;
}

void rs_tag_set_string(RSTag* self, const char* str)
{
    rs_return_if_fail(self && self->type == RS_TAG_STRING);
    char* oldstring = self->string;
    self->string = rs_strdup(str);
    if (oldstring)
        rs_free(oldstring);
}

void rs_tag_list_iterator_init(RSTag* self, RSTagIterator* it)
{
    rs_return_if_fail(self && self->type == RS_TAG_LIST);
    rs_return_if_fail(it);
    
    *it = self->list.items;
}

bool rs_tag_list_iterator_next(RSTagIterator* it, RSTag** tag)
{
    rs_return_val_if_fail(it, false);
    rs_return_val_if_fail(tag, false);
    
    RSList* cell = (RSList*)(*it);
    if (!cell)
        return false;
    
    *tag = (RSTag*)(cell->data);
    *it = cell->next;
    
    return true;
}

RSTagType rs_tag_list_get_type(RSTag* self)
{
    rs_return_val_if_fail(self && self->type == RS_TAG_LIST, RS_TAG_END);
    return self->list.type;
}

void rs_tag_list_set_type(RSTag* self, RSTagType type)
{
    rs_return_if_fail(self && self->type == RS_TAG_LIST);
    if (self->list.items != NULL)
    {
        rs_critical("rs_tag_list_set_type called on non-empty list");
        return;
    }
    self->list.type = type;
}

uint32_t rs_tag_list_get_length(RSTag* self)
{
    rs_return_val_if_fail(self && self->type == RS_TAG_LIST, 0);
    return rs_list_size(self->list.items);
}

RSTag* rs_tag_list_get(RSTag* self, uint32_t i)
{
    rs_return_val_if_fail(self && self->type == RS_TAG_LIST, NULL);
    return (RSTag*)rs_list_nth(self->list.items, i);
}

void rs_tag_list_delete(RSTag* self, uint32_t i)
{
    rs_return_if_fail(self && self->type == RS_TAG_LIST);
    
    RSList* cell = rs_list_nth_cell(self->list.items, i);
    if (cell)
    {
        rs_tag_unref((RSTag*)(cell->data));
        self->list.items = rs_list_remove(self->list.items, cell);
    }
}

void rs_tag_list_insert(RSTag* self, uint32_t i, RSTag* tag)
{
    rs_return_if_fail(self && self->type == RS_TAG_LIST);
    rs_return_if_fail(tag);
    rs_return_if_fail(tag->type == self->list.type);
    
    RSList* cell = rs_list_cell_new();
    
    rs_tag_ref(tag);
    cell->data = tag;
    cell->next = NULL;
    
    if (i == 0)
    {
        cell->next = self->list.items;
        self->list.items = cell;
        return;
    }
    
    uint32_t len = rs_tag_list_get_length(self);
    if (i >= len)
    {
        RSList* last = rs_list_nth_cell(self->list.items, len - 1);
        last->next = cell;
    } else {
        RSList* prev = rs_list_nth_cell(self->list.items, i - 1);
        RSList* next = prev->next;
        
        prev->next = cell;
        cell->next = next;
    }
}

void rs_tag_list_reverse(RSTag* self)
{
    rs_return_if_fail(self && self->type == RS_TAG_LIST);
    self->list.items = rs_list_reverse(self->list.items);
}       

/* for compounds */
void rs_tag_compound_iterator_init(RSTag* self, RSTagIterator* it)
{
    rs_return_if_fail(self && self->type == RS_TAG_COMPOUND);
    rs_return_if_fail(it);
    
    *it = self->compound;
}

bool rs_tag_compound_iterator_next(RSTagIterator* it, const char** key, RSTag** value)
{
    rs_return_val_if_fail(it, false);
    
    RSList* cell = (RSList*)(*it);
    if (!cell)
        return false;
    
    RSTagCompoundNode* node = (RSTagCompoundNode*)(cell->data);
    
    if (key)
        *key = node->key;
    if (value)
        *value = node->value;
    
    *it = cell->next;
    
    return true;
}

uint32_t rs_tag_compound_get_length(RSTag* self)
{
    rs_return_val_if_fail(self && self->type == RS_TAG_COMPOUND, 0);
    return rs_list_size(self->compound);
}

RSTag* rs_tag_compound_get(RSTag* self, const char* key)
{
    rs_return_val_if_fail(self && self->type == RS_TAG_COMPOUND, NULL);
    rs_return_val_if_fail(key, NULL);
    
    RSList* cell = self->compound;
    for (; cell != NULL; cell = cell->next)
    {
        RSTagCompoundNode* node = (RSTagCompoundNode*)(cell->data);
        rs_assert(node && node->key);
        
        if (strcmp(node->key, key) == 0)
            return node->value;
    }
    
    return NULL;
}

RSTag* rs_tag_compound_get_chainv(RSTag* self, va_list ap)
{
    rs_return_val_if_fail(self && self->type == RS_TAG_COMPOUND, NULL);

    const char* key;
    RSTag* tag = self;
    while (tag && (key = va_arg(ap, const char*)))
    {
        if (tag->type != RS_TAG_COMPOUND)
        {
            return NULL;
        }
        tag = rs_tag_compound_get(tag, key);
    }
    
    return tag;
}

RSTag* rs_tag_compound_get_chain(RSTag* self, ...)
{
    va_list ap;
    RSTag* tag;
    
    va_start(ap, self);
    tag = rs_tag_compound_get_chainv(self, ap);
    va_end(ap);
    
    return tag;
}

void rs_tag_compound_set(RSTag* self, const char* key, RSTag* value)
{
    rs_return_if_fail(self && self->type == RS_TAG_COMPOUND);
    rs_return_if_fail(key && value);
    
    rs_tag_compound_delete(self, key);
    
    RSTagCompoundNode* node = rs_new0(RSTagCompoundNode, 1);
    
    node->key = rs_strdup(key);
    rs_tag_ref(value);
    node->value = value;
    
    self->compound = rs_list_push(self->compound, node);
}

void rs_tag_compound_delete(RSTag* self, const char* key)
{
    rs_return_if_fail(self && self->type == RS_TAG_COMPOUND);
    rs_return_if_fail(key);
    
    RSList* cell = self->compound;
    for (; cell != NULL; cell = cell->next)
    {
        RSTagCompoundNode* node = (RSTagCompoundNode*)(cell->data);
        rs_assert(node && node->key);
        
        if (strcmp(node->key, key) == 0)
            break;
    }
    
    if (cell)
        self->compound = rs_list_remove(self->compound, cell);
}
