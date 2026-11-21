// The guest XINPUT_CAPABILITIES layout is packed to 25 bytes; resolve only host-owned handles.
X86_FUNC(sub_0048E398) {
    const uint32_t handle=kernel::R32(ctx.esp+4), destination=kernel::R32(ctx.esp+8);
    uint32_t port=0;
    {
        std::lock_guard<std::mutex> guard(g_ports_lock);
        auto it=g_handles.find(handle);
        if (it==g_handles.end()) {kernel::Return(ctx,6,2);return;}
        port=it->second.port;
    }
    if (!input::Poll(int(port)).connected) {kernel::Return(ctx,1167,2);return;}
    if (!destination || destination>0xFFFFFFFFu-24 || !mem::IsCommitted(destination) || !mem::IsCommitted(destination+24)) {
        kernel::Return(ctx,87,2);return;
    }
    uint8_t caps[25]{};
    caps[0]=1; // standard gamepad
    caps[3]=0xFF; // D-pad, START/BACK and both thumb clicks
    for (int i=5;i<13;++i) caps[i]=255; // A/B/X/Y/black/white/LT/RT
    for (int i=13;i<21;i+=2) {caps[i]=255;caps[i+1]=127;}
    // The game reads motors only when a port appears. The keyboard keeps port 0 connected, so a
    // controller detected later never refreshes them; the host drops vibration without a motor.
    for(int i=21;i<25;++i) caps[i]=255;
    std::memcpy(kernel::HostPtr(destination),caps,sizeof(caps));
    LOG_FIRST(4,"openshao input: capabilities port=%u motors=%u",port,unsigned(caps[21]));
    kernel::Return(ctx,0,2);
}
