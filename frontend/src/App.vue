<script setup>
import AppSidebar from './components/AppSidebar.vue'
import ParticlePhotoOverlay from './components/effects/ParticlePhotoOverlay.vue'
</script>

<template>
  <ParticlePhotoOverlay />
  <div class="app-shell">
    <AppSidebar />
    <main class="app-main">
      <router-view v-slot="{ Component }">
        <Transition name="page" mode="out-in">
          <component :is="Component" />
        </Transition>
      </router-view>
    </main>
  </div>
</template>
