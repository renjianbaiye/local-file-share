<script setup>
defineProps({
  group: { type: Object, required: true },
})
const emit = defineEmits(['expand'])

const bestPhoto = (group) => group.photos?.find(p => p.is_best) || group.photos?.[0]
</script>

<template>
  <article class="photo-stack-card" @click="$emit('expand', group)">
    <div class="stack-visual">
      <!-- Stacked cards behind -->
      <div
        v-for="(photo, i) in group.photos.slice(1, 3).reverse()"
        :key="photo.id"
        class="stack-behind"
        :style="{ '--stack-offset': (2 - i) }"
      >
        <img :src="photo.thumbnailUrl" :alt="photo.fileName" loading="lazy" />
      </div>
      <!-- Top card (best photo) -->
      <div class="stack-top">
        <img :src="bestPhoto(group)?.thumbnailUrl" :alt="bestPhoto(group)?.fileName" loading="lazy" />
        <span class="stack-badge">{{ group.photos.length }}</span>
      </div>
    </div>
    <div class="stack-info">
      <h4>{{ group.title }}</h4>
      <p>
        推荐保留 <strong>{{ group.recommended_keep }}</strong> 张 ·
        待删除候选 <strong>{{ group.delete_candidates }}</strong> 张
      </p>
    </div>
  </article>
</template>
