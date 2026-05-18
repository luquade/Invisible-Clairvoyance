#include "log.h"

void PatchClairvoyance()
{
    // 0x21143 is the FormID for Clairvoyance in Skyrim.esm
    auto clairvoyance = RE::TESForm::LookupByID<RE::SpellItem>(0x21143);

    if (clairvoyance) {
        for (auto& effect : clairvoyance->effects) {
            if (effect && effect->baseEffect) {
                // 1. Mechanical Invisibility (Bypasses AI "hearing" and hostile checks)
                effect->baseEffect->data.castingSoundLevel = RE::SOUND_LEVEL::kSilent;
                effect->baseEffect->data.flags.reset(RE::EffectSetting::EffectSettingData::Flag::kHostile);
                effect->baseEffect->data.flags.reset(RE::EffectSetting::EffectSettingData::Flag::kDetrimental);

                // 2. Visual Invisibility (Targeting "Real fancy" comments)
                // Removes the hand glow and hit-marker art that scripts often watch for.
                effect->baseEffect->data.castingArt = nullptr;
                effect->baseEffect->data.hitEffectArt = nullptr;

                // 3. Surgical Audio Muting (Verified via MagicSystem.h)
                // Mute wind-up (kCharge) and initial whoosh (kRelease).
                for (auto& soundPair : effect->baseEffect->effectSounds) {
                    if (soundPair.id == RE::MagicSystem::SoundID::kCharge ||
                        soundPair.id == RE::MagicSystem::SoundID::kRelease) {

                        soundPair.sound = nullptr;
                    }
                    // kCastLoop remains valid, so you keep the "active" hum.
                }
            }
        }
        SKSE::log::info("Clairvoyance patched: Visuals and start-sounds muted. Loop sound preserved.");
    }
    else {
        SKSE::log::error("Failed to find Clairvoyance spell record (0x21143).");
    }
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
    switch (a_msg->type) {
    case SKSE::MessagingInterface::kDataLoaded:
        PatchClairvoyance();
        break;
    default:
        break;
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);
    SetupLog();

    SKSE::AllocTrampoline(64);

    auto messaging = SKSE::GetMessagingInterface();
    if (!messaging->RegisterListener("SKSE", MessageHandler)) {
        return false;
    }

    SKSE::log::info("Clairvoyance Silence plugin loaded.");

    return true;
}