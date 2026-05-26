<template>
  <div class="loading-skeleton">
    <div class="skel-row" v-for="i in rows" :key="i">
      <div class="skel-block" v-for="j in cols" :key="j" :style="{ height: randomHeight() + 'px' }" />
    </div>
  </div>
</template>

<script setup>
defineProps({
  rows: { type: Number, default: 3 },
  cols: { type: Number, default: 4 },
})

const randomHeight = () => 140 + Math.floor(Math.random() * 100)
</script>
