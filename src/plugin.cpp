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

void CreateInvisibleClairvoyance()
{
    // 1. Look up the vanilla Clairvoyance spell record (0x21143)
    auto vanillaClairvoyance = RE::TESForm::LookupByID<RE::SpellItem>(0x21143);

    if (vanillaClairvoyance) {
        // 2. Clone the outer spell structure
        auto duplicatedForm = vanillaClairvoyance->CreateDuplicateForm(true, nullptr);
        if (!duplicatedForm) return;

        auto invisibleClairvoyance = duplicatedForm->As<RE::SpellItem>();
        if (!invisibleClairvoyance) return;

        // --- ENGINE-RESERVED FORMID FOR MAXIMUM COLLISION SAFETY ---
        invisibleClairvoyance->SetFormID(0xDE143800, false);
        invisibleClairvoyance->SetFormEditorID("InvisibleClairvoyancePower");
        invisibleClairvoyance->fullName = "Invisible Clairvoyance";

        // 3. Mutate ONLY the clone into a stealthy Lesser Power structure
        invisibleClairvoyance->data.spellType = RE::MagicSystem::SpellType::kLesserPower;
        invisibleClairvoyance->data.castingType = RE::MagicSystem::CastingType::kFireAndForget;
        invisibleClairvoyance->data.delivery = RE::MagicSystem::Delivery::kSelf;

        invisibleClairvoyance->data.flags.set(RE::SpellItem::SpellFlag::kPCStartSpell);
        invisibleClairvoyance->data.flags.set(RE::SpellItem::SpellFlag::kInstantCast);
        invisibleClairvoyance->data.flags.set(RE::SpellItem::SpellFlag::kCostOverride);
        invisibleClairvoyance->data.costOverride = 0;

        for (auto& effect : invisibleClairvoyance->effects) {
            if (effect && effect->baseEffect) {
                // --- DEEP RECORD CLONING ---
                // Native duration is preserved perfectly from the vanilla record copy
                auto duplicatedEffect = effect->baseEffect->CreateDuplicateForm(true, nullptr);
                if (duplicatedEffect) {
                    auto clonedBaseEffect = duplicatedEffect->As<RE::EffectSetting>();
                    if (clonedBaseEffect) {
                        effect->baseEffect = clonedBaseEffect;
                        effect->baseEffect->data.associatedSkill = RE::ActorValue::kIllusion;

                        using EffectFlag = RE::EffectSetting::EffectSettingData::Flag;
                        effect->baseEffect->data.flags.set(static_cast<EffectFlag>(0x08)); // Horseback casting mask
                        effect->baseEffect->data.flags.reset(EffectFlag::kHostile);

                        effect->baseEffect->data.castingArt = nullptr;
                        effect->baseEffect->data.hitEffectArt = nullptr;
                        effect->baseEffect->data.castingSoundLevel = RE::SOUND_LEVEL::kSilent;

                        for (auto& soundPair : effect->baseEffect->effectSounds) {
                            soundPair.sound = nullptr;
                        }
                    }
                }
            }
        }
    }
}

void CheckPlayerSpellbook()
{
    auto invisibleClairvoyance = RE::TESForm::LookupByID<RE::SpellItem>(0xDE143800);
    auto vanillaClairvoyance = RE::TESForm::LookupByID<RE::SpellItem>(0x21143);
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
        CreateInvisibleClairvoyance();

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
