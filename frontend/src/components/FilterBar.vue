<script setup>
defineProps({
  filters: { type: Array, default: () => [] },
  active: { type: String, default: '' },
})
const emit = defineEmits(['change'])
</script>

<template>
  <div class="filter-bar">
    <button
      v-for="f in filters"
      :key="f.value"
      class="filter-pill"
      :class="{ active: active === f.value }"
      @click="$emit('change', f.value)"
    >
      {{ f.label }}
    </button>
  </div>
</template>
