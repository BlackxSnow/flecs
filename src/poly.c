#include "private_api.h"

static const char* mixin_kind_str[] = {
    [EcsMixinBase] = "base (should never be requested by application)",
    [EcsMixinWorld] = "world",
    [EcsMixinEntity] = "entity",
    [EcsMixinObservable] = "observable",
    [EcsMixinIterable] = "iterable",
    [EcsMixinDtor] = "dtor",
    [EcsMixinMax] = "max (should never be requested by application)"
};

ecs_mixins_t ecs_world_t_mixins = {
    .type_name = "ecs_world_t",
    .elems = {
        [EcsMixinWorld] = offsetof(ecs_world_t, self),
        [EcsMixinObservable] = offsetof(ecs_world_t, observable),
        [EcsMixinIterable] = offsetof(ecs_world_t, iterable)
    }
};

ecs_mixins_t ecs_stage_t_mixins = {
    .type_name = "ecs_stage_t",
    .elems = {
        [EcsMixinBase] = offsetof(ecs_stage_t, world),
        [EcsMixinWorld] = offsetof(ecs_stage_t, world)
    }
};

ecs_mixins_t ecs_query_t_mixins = {
    .type_name = "ecs_query_t",
    .elems = {
        [EcsMixinWorld] = offsetof(ecs_query_t, world),
        [EcsMixinEntity] = offsetof(ecs_query_t, entity),
        [EcsMixinIterable] = offsetof(ecs_query_t, iterable),
        [EcsMixinDtor] = offsetof(ecs_query_t, dtor)
    }
};

ecs_mixins_t ecs_trigger_t_mixins = {
    .type_name = "ecs_trigger_t",
    .elems = {
        [EcsMixinWorld] = offsetof(ecs_trigger_t, world),
        [EcsMixinEntity] = offsetof(ecs_trigger_t, entity),
        [EcsMixinDtor] = offsetof(ecs_trigger_t, dtor)
    }
};

ecs_mixins_t ecs_observer_t_mixins = {
    .type_name = "ecs_observer_t",
    .elems = {
        [EcsMixinWorld] = offsetof(ecs_observer_t, world),
        [EcsMixinEntity] = offsetof(ecs_observer_t, entity),
        [EcsMixinDtor] = offsetof(ecs_observer_t, dtor)
    }
};

ecs_mixins_t ecs_filter_t_mixins = {
    .type_name = "ecs_filter_t",
    .elems = {
        [EcsMixinIterable] = offsetof(ecs_filter_t, iterable)
    }
};

static
void* get_mixin(
    const ecs_poly_t *poly,
    ecs_mixin_kind_t kind)
{
    ecs_assert(poly != NULL, ECS_INVALID_PARAMETER, NULL);
    ecs_assert(kind < EcsMixinMax, ECS_INVALID_PARAMETER, NULL);
    
    const ecs_header_t *hdr = poly;
    ecs_assert(hdr != NULL, ECS_INVALID_PARAMETER, NULL);
    ecs_assert(hdr->magic == ECS_OBJECT_MAGIC, ECS_INVALID_PARAMETER, NULL);

    const ecs_mixins_t *mixins = hdr->mixins;
    if (!mixins) {
        /* Object has no mixins */
        goto not_found;
    }

    ecs_size_t offset = mixins->elems[kind];
    if (offset == 0) {
        /* Object has mixins but not the requested one. Try to find the mixin
         * in the poly's base */
        goto find_in_base;
    }

    /* Object has mixin, return its address */
    return ECS_OFFSET(hdr, offset);

find_in_base:    
    if (offset) {
        /* If the poly has a base, try to find the mixin in the base */
        ecs_poly_t *base = *(ecs_poly_t**)ECS_OFFSET(hdr, offset);
        if (base) {
            return get_mixin(base, kind);
        }
    }
    
not_found:
    /* Mixin wasn't found for poly */
    return NULL;
}

static
void* assert_mixin(
    const ecs_poly_t *poly,
    ecs_mixin_kind_t kind)
{
    void *ptr = get_mixin(poly, kind);
    if (!ptr) {
        const ecs_header_t *header = poly;
        const ecs_mixins_t *mixins = header->mixins;
        ecs_err("%s not available for type %s", 
            mixin_kind_str[kind],
            mixins ? mixins->type_name : "unknown");
        ecs_os_abort();
    }

    return ptr;
}

void* _ecs_poly_init(
    ecs_poly_t *poly,
    int32_t type,
    ecs_size_t size,
    ecs_mixins_t *mixins)
{
    ecs_assert(poly != NULL, ECS_INVALID_PARAMETER, NULL);

    ecs_header_t *hdr = poly;
    ecs_os_memset(poly, 0, size);

    hdr->magic = ECS_OBJECT_MAGIC;
    hdr->type = type;
    hdr->mixins = mixins;

    return poly;
}

void _ecs_poly_fini(
    ecs_poly_t *poly,
    int32_t type)
{
    ecs_assert(poly != NULL, ECS_INVALID_PARAMETER, NULL);
    (void)type;

    ecs_header_t *hdr = poly;

    /* Don't deinit poly that wasn't initialized */
    ecs_assert(hdr->magic == ECS_OBJECT_MAGIC, ECS_INVALID_PARAMETER, NULL);
    ecs_assert(hdr->type == type, ECS_INVALID_PARAMETER, NULL);
    hdr->magic = 0;
}

EcsPoly* _ecs_poly_bind(
    ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t tag)
{
    /* Add tag to the entity for easy querying. This will make it possible to
     * query for `Query` instead of `(Poly, Query) */
    ecs_add_id(world, entity, tag);

    /* If this is a new poly, leave the actual creation up to the caller so they
     * call tell the difference between a create or an update */
    return ecs_get_mut_pair(world, entity, EcsPoly, tag, NULL);
}

const EcsPoly* _ecs_poly_bind_get(
    const ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t tag)
{
    return ecs_get_pair(world, entity, EcsPoly, tag);
}

ecs_poly_t* _ecs_poly_get(
    const ecs_world_t *world,
    ecs_entity_t entity,
    ecs_entity_t tag)
{
    const EcsPoly *p = _ecs_poly_bind_get(world, entity, tag);
    if (p) {
        return p->poly;
    }
    return NULL;
}

ecs_entity_t ecs_poly_entity_init(
    ecs_world_t *world,
    const ecs_entity_desc_t *desc)
{
    ecs_entity_t existing = desc->entity;
    ecs_entity_t result = ecs_entity_init(world, desc);
    if (!existing && (ecs_get_name(world, result) == NULL)) {
        /* If not associated with an existing entity and is anonymous, add it to
         * the flecs.hidden scope so it won't clutter up the root. */
        ecs_add_pair(world, result, EcsChildOf, EcsFlecsHidden);
    }
    return result;
}

#define assert_object(cond, file, line, type_name)\
    _ecs_assert((cond), ECS_INVALID_PARAMETER, #cond, file, line, type_name);\
    assert(cond)

#ifndef FLECS_NDEBUG
void* _ecs_poly_assert(
    const ecs_poly_t *poly,
    int32_t type,
    const char *file,
    int32_t line)
{
    assert_object(poly != NULL, file, line, 0);
    
    const ecs_header_t *hdr = poly;
    const char *type_name = hdr->mixins->type_name;
    assert_object(hdr->magic == ECS_OBJECT_MAGIC, file, line, type_name);
    assert_object(hdr->type == type, file, line, type_name);
    return (ecs_poly_t*)poly;
}
#endif

bool _ecs_poly_is(
    const ecs_poly_t *poly,
    int32_t type)
{
    ecs_assert(poly != NULL, ECS_INVALID_PARAMETER, NULL);

    const ecs_header_t *hdr = poly;
    ecs_assert(hdr->magic == ECS_OBJECT_MAGIC, ECS_INVALID_PARAMETER, NULL);
    return hdr->type == type;    
}

ecs_iterable_t* ecs_get_iterable(
    const ecs_poly_t *poly)
{
    return (ecs_iterable_t*)assert_mixin(poly, EcsMixinIterable);
}

ecs_observable_t* ecs_get_observable(
    const ecs_poly_t *poly)
{
    return (ecs_observable_t*)assert_mixin(poly, EcsMixinObservable);
}

const ecs_world_t* ecs_get_world(
    const ecs_poly_t *poly)
{
    return *(ecs_world_t**)assert_mixin(poly, EcsMixinWorld);
}

ecs_poly_dtor_t* ecs_get_dtor(
    const ecs_poly_t *poly)
{
    return (ecs_poly_dtor_t*)assert_mixin(poly, EcsMixinDtor);
}
