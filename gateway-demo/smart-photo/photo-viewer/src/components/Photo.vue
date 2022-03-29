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
      api_url: 'http://localhost:8080/cgi-bin/smartphoto',
      picture_server: 'http://localhost:8080/smartphoto/'
    }
  },

  mounted() {
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
        vc.images.length = 0
        let s = vc.picture_server + '/photos/'
        let t = vc.picture_server + '/thumbnail/crop_'

        for (let p of response.data.photos) {
          let filename = p.path.replace(/^.*(\\|\/)/, '')
          vc.images.push({ image: s+filename, thumb: t+filename, });
        }
      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    } else if (type == 'people') {
      axios.post(this.api_url, {
        method: 'list_photo_by_person',
        param: id,
      }).then(function (response) {
        console.log('### list_photo_by_person response:', response)

        vc.images.length = 0

        let s = vc.picture_server + '/photos/'
        let t = vc.picture_server + '/thumbnail/crop_'

        for (let p of response.data.photos) {
          let filename = p.path.replace(/^.*(\\|\/)/, '')

          vc.images.push({ image: s+filename, thumb: t+filename, });
        }

      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    } else if (type == 'all') {
      axios.post(this.api_url, {
        method: 'list_all_photo',
        param: id,
      }).then(function (response) {
        console.log('### list_photo_by_person response:', response)

        vc.images.length = 0

        let s = vc.picture_server + '/photos/'
        let t = vc.picture_server + '/thumbnail/crop_'

        for (let p of response.data.photos) {
          let filename = p.path.replace(/^.*(\\|\/)/, '')

          vc.images.push({ image: s+filename, thumb: t+filename, });
        }

      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    }
  },
}

</script>

<style scoped>
</style>

