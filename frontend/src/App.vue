<script setup>
import { onMounted, onBeforeUnmount, ref } from 'vue'
import AppSidebar from './components/AppSidebar.vue'

/* ── Mouse Tracking for Glass Effects ── */
const mouseX = ref(0)
const mouseY = ref(0)

const handleGlobalMouseMove = (e) => {
  mouseX.value = e.clientX
  mouseY.value = e.clientY
  document.documentElement.style.setProperty('--mouse-x', `${e.clientX}px`)
  document.documentElement.style.setProperty('--mouse-y', `${e.clientY}px`)
}

onMounted(() => {
  window.addEventListener('mousemove', handleGlobalMouseMove)
})

onBeforeUnmount(() => {
  window.removeEventListener('mousemove', handleGlobalMouseMove)
})
</script>

<template>
  <!-- Ambient Background — Floating Gradient Orbs -->
  <div class="ambient-bg">
    <div class="gradient-orb orb-1"></div>
    <div class="gradient-orb orb-2"></div>
    <div class="gradient-orb orb-3"></div>
  </div>

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
