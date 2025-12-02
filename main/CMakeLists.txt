idf_component_register(SRCS "main.c" "audio_config_app/audio_config_app.c"
                       PRIV_REQUIRES xn_lottie_manager xn_dice_app xn_audio_manager xn_audio_prompt
                       INCLUDE_DIRS "." "audio_config_app")
