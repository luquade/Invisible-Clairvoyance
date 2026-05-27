#include "log.h"

// Track what type of notification needs to display once the UI is ready
enum class NotificationState {
    kNone,
    kLearned,
    kRemoved
};
NotificationState g_NotificationType = NotificationState::kNone;

class LoadingMenuSink : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
{
public:
    static LoadingMenuSink* GetSingleton()
    {
        static LoadingMenuSink singleton;
        return &singleton;
    }

    RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
    {
        if (!a_event) return RE::BSEventNotifyControl::kContinue;

        if (a_event->menuName == "Loading Menu" && !a_event->opening) {
            if (g_NotificationType == NotificationState::kLearned) {
                RE::DebugNotification("Minor Power: Invisible Clairvoyance learned.");
                g_NotificationType = NotificationState::kNone;
            }
            else if (g_NotificationType == NotificationState::kRemoved) {
                RE::DebugNotification("Minor Power: Invisible Clairvoyance forgotten.");
                g_NotificationType = NotificationState::kNone;
            }
        }
        return RE::BSEventNotifyControl::kContinue;
    }
};

// Setup function that modifies the ESL stub spell
void SetupInvisibleClairvoyance()
{
    // 1. Look up the vanilla Clairvoyance spell by its EditorID
    auto vanillaClairvoyance = RE::TESForm::LookupByEditorID<RE::SpellItem>("Clairvoyance");
    // 2. Look up the ESL stub spell we created in the Creation Kit / xEdit
    auto invisibleClairvoyance = RE::TESForm::LookupByEditorID<RE::SpellItem>("InvisibleClairvoyancePower_STUB");

    if (!vanillaClairvoyance || !invisibleClairvoyance) {
        // One of them doesn't exist – something is wrong with the installation
        return;
    }

    // Mutate the stub into a stealthy Lesser Power
    invisibleClairvoyance->data.spellType = RE::MagicSystem::SpellType::kLesserPower;
    invisibleClairvoyance->data.castingType = RE::MagicSystem::CastingType::kFireAndForget;
    invisibleClairvoyance->data.delivery = RE::MagicSystem::Delivery::kSelf;

    invisibleClairvoyance->data.flags.set(RE::SpellItem::SpellFlag::kPCStartSpell);
    invisibleClairvoyance->data.flags.set(RE::SpellItem::SpellFlag::kInstantCast);
    invisibleClairvoyance->data.flags.set(RE::SpellItem::SpellFlag::kCostOverride);
    invisibleClairvoyance->data.costOverride = 0;

    // Set the display name
    invisibleClairvoyance->fullName = "Invisible Clairvoyance";

    // Clear any existing effects that the stub might have (safety)
    invisibleClairvoyance->effects.clear();

    // Clone each base effect from the original Clairvoyance
    for (auto& vanillaEffect : vanillaClairvoyance->effects) {
        if (!vanillaEffect || !vanillaEffect->baseEffect) continue;

        // Create a new Effect structure for our stub
        RE::Effect* newEffect = new RE::Effect();
        *newEffect = *vanillaEffect;  // copies magnitude, area, duration, etc.

        // Deep record cloning of the base effect
        auto duplicatedEffect = vanillaEffect->baseEffect->CreateDuplicateForm(true, nullptr);
        if (duplicatedEffect) {
            auto clonedBaseEffect = duplicatedEffect->As<RE::EffectSetting>();
            if (clonedBaseEffect) {
                newEffect->baseEffect = clonedBaseEffect;

                // Make the effect silent and non-hostile
                clonedBaseEffect->data.associatedSkill = RE::ActorValue::kIllusion;

                using EffectFlag = RE::EffectSetting::EffectSettingData::Flag;
                clonedBaseEffect->data.flags.set(static_cast<EffectFlag>(0x08)); // Horseback casting mask
                clonedBaseEffect->data.flags.reset(EffectFlag::kHostile);

                clonedBaseEffect->data.castingArt = nullptr;
                clonedBaseEffect->data.hitEffectArt = nullptr;
                clonedBaseEffect->data.castingSoundLevel = RE::SOUND_LEVEL::kSilent;

                for (auto& soundPair : clonedBaseEffect->effectSounds) {
                    soundPair.sound = nullptr;
                }
            }
        }

        // Add the newly created effect to our stub spell
        invisibleClairvoyance->effects.push_back(newEffect);
    }
}

void CheckPlayerSpellbook()
{
    auto invisibleClairvoyance = RE::TESForm::LookupByEditorID<RE::SpellItem>("InvisibleClairvoyancePower_STUB");
    auto vanillaClairvoyance = RE::TESForm::LookupByEditorID<RE::SpellItem>("Clairvoyance");
    auto player = RE::PlayerCharacter::GetSingleton();

    if (player && vanillaClairvoyance && invisibleClairvoyance) {
        if (player->HasSpell(vanillaClairvoyance)) {
            if (!player->HasSpell(invisibleClairvoyance)) {
                player->AddSpell(invisibleClairvoyance);
                g_NotificationType = NotificationState::kLearned;
            }
        }
        else {
            if (player->HasSpell(invisibleClairvoyance)) {
                player->RemoveSpell(invisibleClairvoyance);
                g_NotificationType = NotificationState::kRemoved;
            }
        }
    }
}

void MessageHandler(SKSE::MessagingInterface::Message* a_msg)
{
    if (a_msg->type == SKSE::MessagingInterface::kDataLoaded) {
        SetupInvisibleClairvoyance();

        auto ui = RE::UI::GetSingleton();
        if (ui) {
            ui->GetEventSource<RE::MenuOpenCloseEvent>()->AddEventSink(LoadingMenuSink::GetSingleton());
        }
    }
    else if (a_msg->type == SKSE::MessagingInterface::kPostLoadGame) {
        CheckPlayerSpellbook();
    }
}

SKSEPluginLoad(const SKSE::LoadInterface* skse) {
    SKSE::Init(skse);

    auto messaging = SKSE::GetMessagingInterface();
    if (messaging) {
        messaging->RegisterListener("SKSE", MessageHandler);
    }

    return true;
}
