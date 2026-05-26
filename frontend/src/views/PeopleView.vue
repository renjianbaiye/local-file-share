<script setup>
import { ref, onMounted } from 'vue'
import { Users } from '@lucide/vue'
import { usePhotos } from '../composables/usePhotos.js'
import EmptyState from '../components/EmptyState.vue'

const { persons, load, loaded, getTagLabel } = usePhotos()
const selectedPerson = ref(null)

onMounted(() => load())

const displayName = (person) => person.name || `未命名人物 ${person.id.replace('person_', '')}`
</script>

<template>
  <div class="view-page people-view">
    <header class="view-header">
      <h1>人物</h1>
      <p>AI 自动识别的人物面孔分组</p>
    </header>

    <EmptyState
      v-if="loaded && persons.length === 0"
      :icon="Users"
      title="暂无人物识别结果"
      description="等待后端生成 person_groups.json 后即可展示人物墙。"
    />

    <div v-else class="people-wall">
      <article
        v-for="person in persons"
        :key="person.id"
        class="person-card"
        :class="{ selected: selectedPerson?.id === person.id }"
        @click="selectedPerson = selectedPerson?.id === person.id ? null : person"
      >
        <div class="person-avatar">
          <img :src="person.avatar" :alt="displayName(person)" />
        </div>
        <h3>{{ displayName(person) }}</h3>
        <span class="person-count">{{ person.photo_count }} 张照片</span>
        <div class="person-tags">
          <span v-for="tag in (person.common_tags || []).slice(0, 3)" :key="tag">
            {{ getTagLabel(tag) }}
          </span>
        </div>
      </article>
    </div>

    <!-- Person detail placeholder -->
    <div v-if="selectedPerson" class="person-detail-placeholder">
      <h2>{{ displayName(selectedPerson) }} 的相册</h2>
      <p>包含 {{ selectedPerson.photo_count }} 张照片 · 等待后端接入真实数据</p>
      <div class="person-detail-actions">
        <button class="action-btn">合并人物</button>
        <button class="action-btn">重命名</button>
      </div>
    </div>
  </div>
</template>
