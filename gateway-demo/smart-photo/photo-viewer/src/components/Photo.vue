<template>
<div style="text-align:center">
  <viewer :images="images">
    <img v-for="(image, index) in images" :src="image.thumb"
     :data-src="image.image" :key="index">
  </viewer>
</div>

</template>

<script>

import axios from 'axios'

export default {
  name: 'Photo',
  data() {
    return {
      images: [],
      api_url: '/cgi-bin/smartphoto',
      picture_server: ''
    }
  },

  mounted() {
    function photos_to_view(vc, photos) {
      vc.images.length = 0
      let s = vc.picture_server
      for (let p of photos) {
        let filename = p.path
        let t = filename.replace(/\/smartphoto\/[^/]+/, '/smartphoto/thumbnail')
        vc.images.push({ image: s+filename, thumb: s+t, });
      }
    }
    console.log('params:', this.$route.params)
    const type = this.$route.params['type']
    const id = this.$route.params['id']
    const vc = this
    if (type == 'categories') {
      console.log('list_photo_by_class')
      axios.post(this.api_url, {
        method: 'list_photo_by_class',
        param: id,
      }).then(function (response) {
        console.log('### list_photo_by_class response:', response)
        photos_to_view(vc, response.data.photos)
      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    } else if (type == 'people') {
      axios.post(this.api_url, {
        method: 'list_photo_by_person',
        param: id,
      }).then(function (response) {
        console.log('### list_photo_by_person response:', response)
        photos_to_view(vc, response.data.photos)
      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    } else if (type == 'all') {
      axios.post(this.api_url, {
        method: 'list_all_photo',
        param: id,
      }).then(function (response) {
        console.log('### list_photo_by_person response:', response)
        photos_to_view(vc, response.data.photos)
      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    }
  },
}

</script>

<style scoped>
</style>

