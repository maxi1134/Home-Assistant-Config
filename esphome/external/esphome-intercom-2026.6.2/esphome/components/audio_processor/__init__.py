import esphome.codegen as cg

# Shared audio processor interface header.
# No config schema: this component only provides the C++ interface.
# esp_aec and esp_afe implement it; esp_audio_stack and intercom_api consume it.

CODEOWNERS = ["@n-IA-hane"]
AUTO_LOAD = ["ring_buffer"]

audio_processor_ns = cg.esphome_ns.namespace("audio_processor")
AudioProcessor = audio_processor_ns.class_("AudioProcessor")


async def to_code(config):
    pass
