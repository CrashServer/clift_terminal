#ifndef INSTALLATION_SCENES_H
#define INSTALLATION_SCENES_H

// Scene function signatures use void* for AudioData and Parameter
// to avoid type definition conflicts with clift_engine.c's anonymous structs.
// The actual .c files cast these to the correct types internally.

// Phase 1: Boot
void scene_boot_sequence(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

// Phase 2: Core procedural scenes
void scene_recovery_scan(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_error_cascade(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_stock_crash(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

void scene_philosophical_drift(char* buffer, float* zbuffer, int width, int height,
                               void* params, float time, void* audio);

// Phase 3: Data-driven scenes
void scene_news_feed(char* buffer, float* zbuffer, int width, int height,
                     void* params, float time, void* audio);

void scene_mastodon_intercept(char* buffer, float* zbuffer, int width, int height,
                              void* params, float time, void* audio);

void scene_wifi_survey(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

// Phase 4: Expanded scenes
void scene_server_mockery(char* buffer, float* zbuffer, int width, int height,
                          void* params, float time, void* audio);

void scene_network_map(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

void scene_wireframe_3d(char* buffer, float* zbuffer, int width, int height,
                        void* params, float time, void* audio);

void scene_roguelike(char* buffer, float* zbuffer, int width, int height,
                     void* params, float time, void* audio);

void scene_big_text(char* buffer, float* zbuffer, int width, int height,
                    void* params, float time, void* audio);

void scene_scifi_terminal(char* buffer, float* zbuffer, int width, int height,
                          void* params, float time, void* audio);

// Phase 6: Massive expansion (12 new scenes)
void scene_surveillance_grid(char* buffer, float* zbuffer, int width, int height,
                             void* params, float time, void* audio);

void scene_cpu_schematic(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_audio_dashboard(char* buffer, float* zbuffer, int width, int height,
                           void* params, float time, void* audio);

void scene_data_transfer(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_wafer_map(char* buffer, float* zbuffer, int width, int height,
                     void* params, float time, void* audio);

void scene_server_room(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

void scene_panopticon(char* buffer, float* zbuffer, int width, int height,
                      void* params, float time, void* audio);

void scene_split_dashboard(char* buffer, float* zbuffer, int width, int height,
                           void* params, float time, void* audio);

void scene_lore_narrative(char* buffer, float* zbuffer, int width, int height,
                          void* params, float time, void* audio);

void scene_hex_dump(char* buffer, float* zbuffer, int width, int height,
                    void* params, float time, void* audio);

void scene_motion_analyzer(char* buffer, float* zbuffer, int width, int height,
                           void* params, float time, void* audio);

void scene_consciousness_stream(char* buffer, float* zbuffer, int width, int height,
                                void* params, float time, void* audio);

// Phase 7: Dynamic action scenes + archived content
void scene_vertical_scroller(char* buffer, float* zbuffer, int width, int height,
                             void* params, float time, void* audio);

void scene_explosion_montage(char* buffer, float* zbuffer, int width, int height,
                             void* params, float time, void* audio);

void scene_cric_meta(char* buffer, float* zbuffer, int width, int height,
                     void* params, float time, void* audio);

void scene_social_feed(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

void scene_propaganda_broadcast(char* buffer, float* zbuffer, int width, int height,
                                void* params, float time, void* audio);

void scene_poetry_display(char* buffer, float* zbuffer, int width, int height,
                          void* params, float time, void* audio);

void scene_faction_war(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

void scene_code_rain_install(char* buffer, float* zbuffer, int width, int height,
                             void* params, float time, void* audio);

void scene_timeline_scroll(char* buffer, float* zbuffer, int width, int height,
                           void* params, float time, void* audio);

void scene_news_wall(char* buffer, float* zbuffer, int width, int height,
                     void* params, float time, void* audio);

void scene_system_overload(char* buffer, float* zbuffer, int width, int height,
                           void* params, float time, void* audio);

void scene_space_battle(char* buffer, float* zbuffer, int width, int height,
                        void* params, float time, void* audio);

// Phase 8: Aggressive philosophy + retro + pulsation scenes
void scene_taz_zone(char* buffer, float* zbuffer, int width, int height,
                    void* params, float time, void* audio);

void scene_retro_arcade(char* buffer, float* zbuffer, int width, int height,
                        void* params, float time, void* audio);

void scene_simulacra(char* buffer, float* zbuffer, int width, int height,
                     void* params, float time, void* audio);

void scene_rhizome(char* buffer, float* zbuffer, int width, int height,
                   void* params, float time, void* audio);

void scene_truth_machine(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_biometric_harvest(char* buffer, float* zbuffer, int width, int height,
                             void* params, float time, void* audio);

void scene_belief_engine(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_tetris_rain(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

void scene_flash_manifesto(char* buffer, float* zbuffer, int width, int height,
                           void* params, float time, void* audio);

void scene_discipline_grid(char* buffer, float* zbuffer, int width, int height,
                           void* params, float time, void* audio);

void scene_pirate_radio(char* buffer, float* zbuffer, int width, int height,
                        void* params, float time, void* audio);

void scene_final_warning(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

// Phase 9: Lore, markets, science, ecosystem (20 scenes)
void scene_crypto_ticker(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_neural_fusion(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_europa_descent(char* buffer, float* zbuffer, int width, int height,
                          void* params, float time, void* audio);

void scene_ecosystem_monitor(char* buffer, float* zbuffer, int width, int height,
                             void* params, float time, void* audio);

void scene_market_meltdown(char* buffer, float* zbuffer, int width, int height,
                           void* params, float time, void* audio);

void scene_barcelona_uprising(char* buffer, float* zbuffer, int width, int height,
                              void* params, float time, void* audio);

void scene_dna_sequencer(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_blockchain_explorer(char* buffer, float* zbuffer, int width, int height,
                               void* params, float time, void* audio);

void scene_love_virus(char* buffer, float* zbuffer, int width, int height,
                      void* params, float time, void* audio);

void scene_particle_accelerator(char* buffer, float* zbuffer, int width, int height,
                                void* params, float time, void* audio);

void scene_edsa_control(char* buffer, float* zbuffer, int width, int height,
                        void* params, float time, void* audio);

void scene_quantum_field_analysis(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_server_diagnostics(char* buffer, float* zbuffer, int width, int height,
                              void* params, float time, void* audio);

void scene_climate_collapse(char* buffer, float* zbuffer, int width, int height,
                            void* params, float time, void* audio);

void scene_crash_voice(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

void scene_spectral_analysis(char* buffer, float* zbuffer, int width, int height,
                             void* params, float time, void* audio);

void scene_network_topology(char* buffer, float* zbuffer, int width, int height,
                            void* params, float time, void* audio);

void scene_memory_palace(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_cosmic_background(char* buffer, float* zbuffer, int width, int height,
                             void* params, float time, void* audio);

void scene_reisub_sequence(char* buffer, float* zbuffer, int width, int height,
                           void* params, float time, void* audio);

// Phase 10: World / Geopolitical scenes
void scene_world_map(char* buffer, float* zbuffer, int width, int height,
                     void* params, float time, void* audio);

void scene_country_intel(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_geopolitical_drift(char* buffer, float* zbuffer, int width, int height,
                              void* params, float time, void* audio);

// Narrative act system — Server as narrator
void scene_server_speaks(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

// Phase 11: ASCII Face & Character scenes (from face-features-lib.js)
void scene_server_face(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

void scene_organic_eye(char* buffer, float* zbuffer, int width, int height,
                       void* params, float time, void* audio);

void scene_face_gallery(char* buffer, float* zbuffer, int width, int height,
                        void* params, float time, void* audio);

void scene_human_figures(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_face_morph(char* buffer, float* zbuffer, int width, int height,
                      void* params, float time, void* audio);

// Phase 12: Live coding performance scenes (Ikeda-style, audio-reactive, full surface)
void scene_live_spectrum(char* buffer, float* zbuffer, int width, int height,
                         void* params, float time, void* audio);

void scene_live_grid(char* buffer, float* zbuffer, int width, int height,
                     void* params, float time, void* audio);

void scene_live_pulse(char* buffer, float* zbuffer, int width, int height,
                      void* params, float time, void* audio);

// Global live coding text (set by clift_engine, read by live scenes)
extern char g_live_code_svdk[4096];
extern char g_live_code_zbdm[4096];
extern bool g_live_code_fresh[2];

// Initialize installation scenes (load lore fragments, etc.)
void installation_scenes_init(void);

#endif // INSTALLATION_SCENES_H
