#include "pch.hpp"

#include <cache/cache.hpp>
#include <logger/logger.hpp>

#include <cache/entities/player.hpp>
#include <cache/entities/weapon.hpp>
#include <cache/entities/hen.hpp>

#include <interfaces/gameentitysystem.hpp>
#include <interfaces/schemasystem.hpp>
#include <interfaces/engineclient.hpp>

#include <bindings/baseentity.hpp>

#define SDK_ENABLE_CACHE_LOGGING

void CMatchCache::Initialize() {
    int highestIdx = CGameEntitySystem::GetHighestEntityIndex();
    for (int i = 1; i <= highestIdx; ++i) {
        C_BaseEntity* ent = CGameEntitySystem::GetBaseEntity(i);
        if (!ent) continue;

        AddEntity(ent, ent->GetRefEHandle());
    }
}

void CMatchCache::AddEntity(CEntityInstance* inst, CBaseHandle handle) {
    // Cache only networked entities.
    // https://developer.valvesoftware.com/wiki/Entity_limit#Source_2_limits

    C_BaseEntity* ent = static_cast<C_BaseEntity*>(inst);
    if (!ent || handle.GetEntryIndex() > 0x3FFF) return;

    CachedEntityPtr cachedEntity = CreateCachedEntityPointer(ent, handle);
    if (!cachedEntity) return;

    {
        const std::lock_guard<std::mutex> lock(m_EntitiesLock);
        m_CachedEntities.emplace(handle.ToInt(), std::move(cachedEntity));
    }

#ifdef SDK_ENABLE_CACHE_LOGGING
    // Log that we've added the entity.
    CSchemaClassInfo* dynamicBinding = ent->Schema_DynamicBinding();
    if (!dynamicBinding) return;

    CLogger::Log("[cache] [+] {} -> {} | CHandle [Serial: {}, Index: {}]", dynamicBinding->m_Name, SDK_LOG_PTR(ent),
                 handle.GetSerialNumber(), handle.GetEntryIndex());
#endif
}

void CMatchCache::RemoveEntity(CEntityInstance* inst, CBaseHandle handle) {
    C_BaseEntity* ent = static_cast<C_BaseEntity*>(inst);
    if (!ent || handle.GetEntryIndex() > 0x3FFF) return;

    auto it = m_CachedEntities.find(handle.ToInt());
    if (it == m_CachedEntities.end()) return;

    {
        const std::lock_guard<std::mutex> lock(m_EntitiesLock);
        m_CachedEntities.erase(it);
    }

#ifdef SDK_ENABLE_CACHE_LOGGING
    // Log that we've removed the entity.
    CSchemaClassInfo* dynamicBinding = ent->Schema_DynamicBinding();
    if (!dynamicBinding) return;

    CLogger::Log("[cache] [-] {} -> {} | CHandle [Serial: {}, Index: {}]", dynamicBinding->m_Name, SDK_LOG_PTR(ent),
                 handle.GetSerialNumber(), handle.GetEntryIndex());
#endif
}

CMatchCache::CachedEntityPtr& CMatchCache::GetEntityByIndex(int i) {
    C_BaseEntity* ent = CGameEntitySystem::GetBaseEntity(i);
    if (ent) {
        auto it = m_CachedEntities.find(ent->GetRefEHandle().ToInt());
        if (it != m_CachedEntities.end()) return it->second;
    }

    static CachedEntityPtr null;
    return null;
}

CCachedPlayer* CMatchCache::GetLocalPlayer() {
    CachedEntityPtr& cachedEntity = GetEntityByIndex(CEngineClient::Get()->GetLocalPlayer());
    if (cachedEntity) {
        return dynamic_cast<CCachedPlayer*>(cachedEntity.get());
    }

    return nullptr;
}

CMatchCache::CachedEntityPtr CMatchCache::CreateCachedEntityPointer(C_BaseEntity* ent, CBaseHandle handle) {
    if (ent->IsPlayerController()) {
        return std::make_unique<CCachedPlayer>(handle);
    } else if (ent->IsWeapon()) {
        return std::make_unique<CCachedWeapon>(handle);
    } else if (ent->IsProjectile()) {
        return std::make_unique<CCachedBaseEntity>(handle);
    } else if (ent->IsPlantedC4()) {
        return std::make_unique<CCachedBaseEntity>(handle);
    } else if (ent->IsChicken()) {
        return std::make_unique<CCachedHen>(handle);
    }

    return nullptr;
}
