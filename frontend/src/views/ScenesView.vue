<script setup>
import { onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { usePhotos } from '../composables/usePhotos.js'
import SceneCard from '../components/SceneCard.vue'
import EmptyState from '../components/EmptyState.vue'
import { Clapperboard } from '@lucide/vue'

const router = useRouter()
const { scenes, load, loaded } = usePhotos()

onMounted(() => load())

const openScene = (scene) => {
  router.push({ name: 'scene-detail', params: { id: scene.id } })
}
</script>

<template>
  <div class="view-page scenes-view">
    <header class="view-header">
      <h1>场景</h1>
      <p>AI 根据照片内容和时间自动归类的场景故事</p>
    </header>

    <EmptyState
      v-if="loaded && scenes.length === 0"
      :icon="Clapperboard"
      title="暂无场景分组"
      description="等待后端生成 scene_groups.json 后即可展示场景卡片。"
    />

    <div v-else class="scenes-grid">
      <SceneCard
        v-for="scene in scenes"
        :key="scene.id"
        :scene="scene"
        @click="openScene"
      />
    </div>
  </div>
</template>
