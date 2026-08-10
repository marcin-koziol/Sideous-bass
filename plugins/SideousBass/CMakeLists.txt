dpf_add_plugin(sideous-bass
  TARGETS lv2 vst3 clap jack
  UI_TYPE cairo
  FILES_DSP
      SideousBassPlugin.cpp
  FILES_UI
      SideousBassUI.cpp)

target_include_directories(sideous-bass PUBLIC ".")
