<template>

<div>
<h2 style="text-align: center;"> SMART PHOTO SEARCH </h2>

<el-divider/>
<div>
<el-container style="text-align:center">
  <el-aside width="200px">
  <el-tree :data="treeData" :props="defaultProps"
   @node-click="handleAsideNodeClick"></el-tree>
  </el-aside>
  <el-container>
    <el-header>
    <el-row>
      <el-button type="primary" @click="onScanStart" plain>Scan Start</el-button>
      <el-button type="danger" plain>Scan Stop</el-button>
      <el-button type="success" @click="onListClass" plain>List Class</el-button>
      <el-button type="warning" @click="onListPerson" plain>List Person</el-button>
    </el-row>
    </el-header>

    <el-main>
    <viewer :images="images">
      <img v-for="(image, index) in images" :src="image.thumb"
       :data-src="image.image" :key="index">
    </viewer>
    </el-main>
  </el-container>
</el-container>
</div>

</div>
</template>

<script>

import axios from 'axios'

export default {
  name: 'Home',
  data() {
    return {
      treeData : [
        { label: 'Class', },
        { label: 'Person', },
        { label: 'All', },
      ],
      defaultProps: {
        children: 'children',
        label: 'label'
      },
      images: [],
      api_url: 'http://ibox.bj.intel.com:8080/cgi-bin/smartphoto',
      /*api_url: 'http://10.238.158.95:8080/cgi-bin/smartphoto',*/
      picture_server: 'http://ibox.bj.intel.com:/smartphoto/'

    }
  },

  methods: {
    personFilter(person) {
      console.log('### person count:', person.count)
      if (person.count < 2)
        return null
      return person
    },
    clssFilter(clssName) {

      /*return clssName*/

      const li = [
        'person',
        'sky',
        'tree',
        'cat',
        'building;edifice',
        'plant;flora;plant;life',
        'mountain;mount',
        'sea',
      ]
      if (li.includes(clssName)) {
        return clssName
      } else {
        return null
      }
    },
    onScanStart() {
      axios.post(this.api_url, {
        method: 'scan_start',
      }).then(function (response) {
        console.log('smartphoto response:', response)
      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    },
    onListClass() {
      let vc = this
      axios.post(this.api_url, {
        method: 'list_class',
      }).then(function (response) {
        console.log('smartphoto response:', response)
        if (response.data.result == 0) {
          let data = vc.treeData[0]

          if (data.children) {
            data.children.length = 0
            console.log('children: ', data.children)
          }

          for (const c of response.data.clsses) {
            let clss = vc.clssFilter(c.name)
            if (clss == null)
              continue
            const child = { label: c.name }
            if (!data.children) {
              vc.$set(data, 'children', [])
            }
            data.children.push(child)
          }
          console.log("treeData[0]", vc.treeData[0])
        }

      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    },
    onListPerson() {
      let vc = this
      axios.post(this.api_url, {
        method: 'list_person',
      }).then(function (response) {
        console.log('smartphoto response:', response)
        if (response.data.result == 0) {
          let data = vc.treeData[1]

          if (data.children) {
            data.children.length = 0
            console.log('children: ', data.children)
          }
          for (const p of response.data.person) {
            const pp = vc.personFilter(p)
            if (pp == null)
              continue
            let child = { label: pp.id }
            if (pp.name) {
              child = { label: pp.name }
            }
            if (!data.children) {
              vc.$set(data, 'children', [])
            }
            data.children.push(child)
          }
          console.log("treeData[1]", vc.treeData[1])
        }

      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    },
    handleAsideNodeClick(data, node) {
      let vc = this
      if (node.level == 1) {
        if (data.label == 'All') {
          console.log("### show all photo")

          axios.post(this.api_url, {
            method: 'list_all_photo',
          }).then(function (response) {
            console.log('### list_all_photo response:', response)

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
      } else if (node.level == 2) {
        let p = node.parent.data.label
        if (p == 'Class') {
          console.log("### list photo by class:", data.label)

          axios.post(this.api_url, {
            method: 'list_photo_by_class',
            param: data.label,
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
        } else if (p == 'Person') {
          console.log("### list photo by person:", data.label)
          axios.post(this.api_url, {
            method: 'list_photo_by_person',
            param: data.label,
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
      }
    },
  },
}

</script>

<style scoped>

</style>

