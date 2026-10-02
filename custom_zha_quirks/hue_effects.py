"""Philips Hue native effects (candle, fireplace, etc.) via cluster 0xFC03."""

from collections.abc import Iterator
from typing import Any, Final

from zha.application.platforms import BaseEntity
from zha.application.platforms.light import HueLight
from zha.application.platforms.light.const import (
    EFFECT_OFF,
    ColorMode,
    LightEntityFeature,
)
import zigpy.types as t
from zigpy.zcl.foundation import BaseCommandDefs, ZCLCommandDef

from zhaquirks.builder import QuirkBuilder
from zhaquirks.builder.device import QuirkV2Device
from zhaquirks.clusters import CustomCluster

PHILIPS = "Philips"
SIGNIFY = "Signify Netherlands B.V."


class HueFrameFlags(t.bitmap16):
    """Fields present in a frame. Wire order of the fields differs from bit order."""

    on_off = 1 << 0
    brightness = 1 << 1
    color_mirek = 1 << 2
    color_xy = 1 << 3
    fade_speed = 1 << 4
    effect_type = 1 << 5
    gradient_params = 1 << 6
    effect_speed = 1 << 7
    gradient_colors = 1 << 8


class HueEffect(t.enum8):
    """Hue effect IDs."""

    No_Effect = 0x00
    Candle = 0x01
    Fireplace = 0x02
    Prism = 0x03
    Sunrise = 0x09
    Sparkle = 0x0A
    Opal = 0x0B
    Glisten = 0x0C
    Sunset = 0x0D
    Underwater = 0x0E
    Cosmos = 0x0F
    Sunbeam = 0x10
    Enchant = 0x11


class HueEffectFrame(t.Struct):
    """Subset of the Hue native control frame, see chrivers/bifrost `hue-zigbee-format.md`."""

    flags: HueFrameFlags
    on_off: t.Bool = t.StructField(
        requires=lambda s: HueFrameFlags.on_off in s.flags
    )
    effect_type: HueEffect = t.StructField(
        requires=lambda s: HueFrameFlags.effect_type in s.flags
    )


class HueEffectCluster(CustomCluster):
    """Philips Hue manufacturer cluster."""

    cluster_id: Final[t.uint16_t] = 0xFC03
    ep_attribute: Final[str] = "philips_hue_effect"
    name: Final[str] = "Philips Hue effect"

    class ServerCommandDefs(BaseCommandDefs):
        """Server command definitions."""

        frame: Final = ZCLCommandDef(
            id=0x00,
            schema={"data": t.Bytes},
            is_manufacturer_specific=True,
        )


EFFECTS: Final[dict[str, HueEffect]] = {
    "candle": HueEffect.Candle,
    "sunrise": HueEffect.Sunrise,
    "sparkle": HueEffect.Sparkle,
    "opal": HueEffect.Opal,
    "glisten": HueEffect.Glisten,
    "sunset": HueEffect.Sunset,
    "underwater": HueEffect.Underwater,
    "cosmos": HueEffect.Cosmos,
    "sunbeam": HueEffect.Sunbeam,
    "enchant": HueEffect.Enchant,
}

COLOR_EFFECTS: Final[dict[str, HueEffect]] = {
    "fireplace": HueEffect.Fireplace,
    "prism": HueEffect.Prism,
}


class HueEffectLight(HueLight):
    """Hue light with native effects."""

    def recompute_capabilities(self) -> None:
        """Add Hue effects to the effect list."""
        super().recompute_capabilities()

        self._hue_effects = dict(EFFECTS)
        if ColorMode.XY in self._internal_supported_color_modes:
            self._hue_effects.update(COLOR_EFFECTS)

        self._supported_features |= LightEntityFeature.EFFECT
        self._effect_list = [*self._effect_list, *self._hue_effects]

    async def _send_frame(self, frame: HueEffectFrame) -> None:
        self.debug("Sending Hue frame: %s", frame)
        await self.endpoint.zigpy_endpoint.philips_hue_effect.frame(
            data=frame.serialize()
        )

    async def _async_turn_on_impl(
        self,
        *,
        effect: str | None,
        color_temp: int | None,
        xy_color: tuple[float, float] | None,
        **kwargs: Any,
    ) -> None:
        await super()._async_turn_on_impl(
            effect=effect, color_temp=color_temp, xy_color=xy_color, **kwargs
        )

        if effect in self._hue_effects:
            await self._send_frame(
                HueEffectFrame(
                    flags=HueFrameFlags.on_off | HueFrameFlags.effect_type,
                    on_off=True,
                    effect_type=self._hue_effects[effect],
                )
            )
            self._effect = effect
        elif effect == EFFECT_OFF and self._effect in self._hue_effects:
            await self._send_frame(
                HueEffectFrame(
                    flags=HueFrameFlags.effect_type,
                    effect_type=HueEffect.No_Effect,
                )
            )
            self._effect = EFFECT_OFF
        elif (
            color_temp is not None or xy_color is not None
        ) and self._effect in self._hue_effects:
            # The bulb stops the effect when the color changes
            self._effect = EFFECT_OFF

        self.maybe_emit_state_changed_event()

    async def async_update(self) -> None:
        """Keep the Hue effect: polling `color_loop_active` would reset it."""
        effect = self._effect
        await super().async_update()

        if effect in self._hue_effects and self._effect == EFFECT_OFF:
            self._effect = effect
            self.maybe_emit_state_changed_event()


class HueEffectDevice(QuirkV2Device):
    """Swap the default Hue light entity for one with native effects."""

    def discover_entities(self) -> Iterator[BaseEntity]:
        """Replace `HueLight` with `HueEffectLight`."""
        for entity in super().discover_entities():
            if type(entity) is HueLight:
                entity = HueEffectLight(
                    endpoint=entity.endpoint,
                    device=self,
                    cluster=entity.cluster,
                )

            yield entity


MODELS: Final = (
    # Color
    "LCA001",
    "LCA002",
    "LCA003",
    "LCA007",
    "LCA009",
    "LCE001",
    "LCT001",
    "LCT007",
    "LCT010",
    "LCT012",
    "LCT014",
    "LCT015",
    "LCT016",
    "LCT021",
    # White
    "LWA008",
)

builder = QuirkBuilder()
for model in MODELS:
    builder.applies_to(PHILIPS, model).applies_to(SIGNIFY, model)

(
    builder.replaces(HueEffectCluster, endpoint_id=11)
    .zha_device_class(HueEffectDevice)
    .add_to_registry()
)
