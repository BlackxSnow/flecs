#include "private_api.h"

static
void table_cache_list_remove(
    ecs_table_cache_t *cache,
    ecs_table_cache_hdr_t *elem)
{
    ecs_table_cache_hdr_t *next = elem->next;
    ecs_table_cache_hdr_t *prev = elem->prev;

    if (next) {
        next->prev = prev;
    }
    if (prev) {
        prev->next = next;
    }

    cache->empty_tables.count -= !!elem->empty;
    cache->tables.count -= !elem->empty;

    if (cache->empty_tables.first == elem) {
        cache->empty_tables.first = next;
    } else if (cache->tables.first == elem) {
        cache->tables.first = next;
    }
    if (cache->empty_tables.last == elem) {
        cache->empty_tables.last = prev;
    }
    if (cache->tables.last == elem) {
        cache->tables.last = prev;
    }
}

static
void table_cache_list_insert(
    ecs_table_cache_t *cache,
    ecs_table_cache_hdr_t *elem)
{
    ecs_table_cache_hdr_t *last;
    if (elem->empty) {
        last = cache->empty_tables.last;
        cache->empty_tables.last = elem;
        if ((++ cache->empty_tables.count) == 1) {
            cache->empty_tables.first = elem;
        }
    } else {
        last = cache->tables.last;
        cache->tables.last = elem;
        if ((++ cache->tables.count) == 1) {
            cache->tables.first = elem;
        }
    }

    elem->next = NULL;
    elem->prev = last;

    if (last) {
        last->next = elem;
    }
}

void ecs_table_cache_init(
    ecs_table_cache_t *cache)
{
    ecs_assert(cache != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_map_init(&cache->index, ecs_table_cache_hdr_t*, 4);
}

void ecs_table_cache_fini(
    ecs_table_cache_t *cache)
{
    ecs_assert(cache != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_map_fini(&cache->index);
}

bool ecs_table_cache_is_empty(
    const ecs_table_cache_t *cache)
{
    return ecs_map_count(&cache->index) == 0;
}

void ecs_table_cache_insert(
    ecs_table_cache_t *cache,
    const ecs_table_t *table,
    ecs_table_cache_hdr_t *result)
{
    ecs_assert(cache != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(ecs_table_cache_get(cache, table) == NULL,
        ECS_INTERNAL_ERROR, NULL);
    ecs_assert(result != NULL, ECS_INTERNAL_ERROR, NULL);

    bool empty;
    if (!table) {
        empty = false;
    } else {
        empty = ecs_table_count(table) == 0;
    }

    result->cache = cache;
    result->table = (ecs_table_t*)table;
    result->empty = empty;

    table_cache_list_insert(cache, result);

    if (table) {
        ecs_map_set_ptr(&cache->index, table->id, result);
    }

    ecs_assert(empty || cache->tables.first != NULL, 
        ECS_INTERNAL_ERROR, NULL);
    ecs_assert(!empty || cache->empty_tables.first != NULL, 
        ECS_INTERNAL_ERROR, NULL);
}

void ecs_table_cache_replace(
    ecs_table_cache_t *cache,
    const ecs_table_t *table,
    ecs_table_cache_hdr_t *elem)
{
    ecs_table_cache_hdr_t **oldptr = ecs_map_get(&cache->index, 
        ecs_table_cache_hdr_t*, table->id);
    ecs_assert(oldptr != NULL, ECS_INTERNAL_ERROR, NULL);

    ecs_table_cache_hdr_t *old = *oldptr;
    ecs_assert(old != NULL, ECS_INTERNAL_ERROR, NULL);

    ecs_table_cache_hdr_t *prev = old->prev, *next = old->next;
    if (prev) {
        ecs_assert(prev->next == old, ECS_INTERNAL_ERROR, NULL);
        prev->next = elem;
    }
    if (next) {
        ecs_assert(next->prev == old, ECS_INTERNAL_ERROR, NULL);
        next->prev = elem;
    }

    if (cache->empty_tables.first == old) {
        cache->empty_tables.first = elem;
    }
    if (cache->empty_tables.last == old) {
        cache->empty_tables.last = elem;
    }
    if (cache->tables.first == old) {
        cache->tables.first = elem;
    }
    if (cache->tables.last == old) {
        cache->tables.last = elem;
    }

    *oldptr = elem;
    elem->prev = prev;
    elem->next = next;
}

void* ecs_table_cache_get(
    const ecs_table_cache_t *cache,
    const ecs_table_t *table)
{
    ecs_assert(cache != NULL, ECS_INTERNAL_ERROR, NULL);
    if (table) {
        return ecs_map_get_ptr(&cache->index, ecs_table_cache_hdr_t*, table->id);
    } else {
        ecs_table_cache_hdr_t *elem = cache->tables.first;
        ecs_assert(!elem || elem->table == NULL, ECS_INTERNAL_ERROR, NULL);
        return elem;
    }
}

void* ecs_table_cache_remove(
    ecs_table_cache_t *cache,
    const ecs_table_t *table,
    ecs_table_cache_hdr_t *elem)
{
    ecs_assert(cache != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(table != NULL, ECS_INTERNAL_ERROR, NULL);

    if (!ecs_map_is_initialized(&cache->index)) {
        return NULL;
    }

    if (!elem) {
        elem = ecs_map_get_ptr(
            &cache->index, ecs_table_cache_hdr_t*, table->id);
        if (!elem) {
            return false;
        }
    }

    ecs_assert(elem != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(elem->cache == cache, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(elem->table == table, ECS_INTERNAL_ERROR, NULL);

    table_cache_list_remove(cache, elem);

    ecs_map_remove(&cache->index, table->id);

    return elem;
}

bool ecs_table_cache_set_empty(
    ecs_table_cache_t *cache,
    const ecs_table_t *table,
    bool empty)
{
    ecs_assert(cache != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(table != NULL, ECS_INTERNAL_ERROR, NULL);

    ecs_table_cache_hdr_t *elem = ecs_map_get_ptr(
        &cache->index, ecs_table_cache_hdr_t*, table->id);
    if (!elem) {
        return false;
    }

    if (elem->empty == empty) {
        return false;
    }

    table_cache_list_remove(cache, elem);
    elem->empty = empty;
    table_cache_list_insert(cache, elem);

    return true;
}

bool flecs_table_cache_iter(
    ecs_table_cache_t *cache,
    ecs_table_cache_iter_t *out)
{
    ecs_assert(cache != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(out != NULL, ECS_INTERNAL_ERROR, NULL);
    out->next = cache->tables.first;
    out->cur = NULL;
    return out->next != NULL;
}

bool flecs_table_cache_empty_iter(
    ecs_table_cache_t *cache,
    ecs_table_cache_iter_t *out)
{
    ecs_assert(cache != NULL, ECS_INTERNAL_ERROR, NULL);
    ecs_assert(out != NULL, ECS_INTERNAL_ERROR, NULL);
    out->next = cache->empty_tables.first;
    out->cur = NULL;
    return out->next != NULL;
}

ecs_table_cache_hdr_t* _flecs_table_cache_next(
    ecs_table_cache_iter_t *it)
{
    ecs_table_cache_hdr_t *next = it->next;
    if (!next) {
        return false;
    }

    it->cur = next;
    it->next = next->next;
    return next;
}
