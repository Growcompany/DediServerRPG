#include "AbilitySystem/DSTRGameplayTags.h"

namespace DSTRGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG(Ability_Attack_Basic, "Ability.Attack.Basic");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Skill_MakeWay, "Ability.Skill.MakeWay");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Skill_Fortify, "Ability.Skill.Fortify");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Skill_Charge, "Ability.Skill.Charge");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Skill_Reckoning, "Ability.Skill.Reckoning");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Interaction_Revive, "Ability.Interaction.Revive");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Reaction_Pulled, "Ability.Reaction.Pulled");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Boss_ColossalBlow, "Ability.Boss.ColossalBlow");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Boss_PhantomRush, "Ability.Boss.PhantomRush");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Boss_Siphon, "Ability.Boss.Siphon");
	UE_DEFINE_GAMEPLAY_TAG(Ability_Boss_Subjugate, "Ability.Boss.Subjugate");

	UE_DEFINE_GAMEPLAY_TAG(InputTag_Ability_BasicAttack, "InputTag.Ability.BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Ability_MakeWay, "InputTag.Ability.MakeWay");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Ability_Fortify, "InputTag.Ability.Fortify");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Ability_Charge, "InputTag.Ability.Charge");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Ability_Reckoning, "InputTag.Ability.Reckoning");
	UE_DEFINE_GAMEPLAY_TAG(InputTag_Ability_Revive, "InputTag.Ability.Revive");

	UE_DEFINE_GAMEPLAY_TAG(State_Dead, "State.Dead");
	UE_DEFINE_GAMEPLAY_TAG(State_Invulnerable, "State.Invulnerable");
	UE_DEFINE_GAMEPLAY_TAG(State_Stunned, "State.Stunned");
	UE_DEFINE_GAMEPLAY_TAG(State_Attacking, "State.Attacking");
	UE_DEFINE_GAMEPLAY_TAG(State_Fortified, "State.Fortified");
	UE_DEFINE_GAMEPLAY_TAG(State_Slowed, "State.Slowed");

	UE_DEFINE_GAMEPLAY_TAG(Effect_Damage, "Effect.Damage");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Duration, "Effect.Duration");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Init_MaxHealth, "Effect.Init.MaxHealth");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Init_AttackPower, "Effect.Init.AttackPower");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Heal, "Effect.Heal");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Buff_Attack, "Effect.Buff.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_Attack, "Effect.Cooldown.Attack");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_MakeWay, "Effect.Cooldown.MakeWay");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_Fortify, "Effect.Cooldown.Fortify");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_Charge, "Effect.Cooldown.Charge");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_Reckoning, "Effect.Cooldown.Reckoning");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_EnemyAttack, "Effect.Cooldown.EnemyAttack");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_BossColossal, "Effect.Cooldown.BossColossal");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_BossRush, "Effect.Cooldown.BossRush");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_BossSiphon, "Effect.Cooldown.BossSiphon");
	UE_DEFINE_GAMEPLAY_TAG(Effect_Cooldown_BossSubjugate, "Effect.Cooldown.BossSubjugate");

	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Hit, "Event.Combat.Hit");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Death, "Event.Combat.Death");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Revive, "Event.Combat.Revive");
	UE_DEFINE_GAMEPLAY_TAG(Event_Combat_Pulled, "Event.Combat.Pulled");

	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_DSTR_BasicAttack, "GameplayCue.DSTR.BasicAttack");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_DSTR_EnemyTelegraph, "GameplayCue.DSTR.EnemyTelegraph");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_DSTR_BossWindup, "GameplayCue.DSTR.BossWindup");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_DSTR_BossRush, "GameplayCue.DSTR.BossRush");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_DSTR_BossSiphon, "GameplayCue.DSTR.BossSiphon");
	UE_DEFINE_GAMEPLAY_TAG(GameplayCue_DSTR_GateSealed, "GameplayCue.DSTR.GateSealed");
}
