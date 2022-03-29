<template>
<div>
<el-container style="text-align:center">

  <el-aside width="200px">
   <el-button style="min-width: 150px;" type="primary" @click="onScanStart" plain>Scan</el-button>
   <p/>
   <el-button style="min-width: 150px;" type="primary" plain>Stop</el-button>
   <p/>
   <el-button style="min-width: 150px;" type="primary" @click="onAllPhotos" plain>All photos</el-button>
   <p/>

   <el-upload
    class="upload-demo"
    :action="upload_action_rul"
    :on-preview="uploadPreview"
    :on-remove="uploadRemove"
    :file-list="uploadFileList"
    list-type="picture">
   <el-button style="min-width: 150px;" type="primary" plain>Add a photo</el-button>
   </el-upload>

  </el-aside>

  <el-container>
    <el-main v-loading="scaning" style="min-height: 100px;">

    <el-row> <el-col :span="4" :offset="0"> Categories </el-col> </el-row>

    <el-row style="display: flex; flex-wrap: wrap;">
    <div v-for="[clss, thumb] in Object.entries(categories)" :key="clss">
      <el-card shadow="hover" style="margin: 4px; max-width: 200px;" :body-style="{ padding: '0px' }">
        <div>
          <img @click="onClickCategory(clss)" :src="thumb" class="image"/>
          <div style="padding: 4px;">
            <span> {{clss}} </span>
          </div>
        </div>
      </el-card>
    </div>
    </el-row>

    <el-row> <el-col :span="24" :offset="0"> <el-divider/> </el-col> </el-row>
    <el-row><el-col :span="4" :offset="0"> People </el-col></el-row>

    <el-row style="display: flex; flex-wrap: wrap;">
    <div :span="4" v-for="[i, thumb] in Object.entries(people)" :key="i">
      <el-card shadow="hover" style="margin: 4px; max-width: 200px;" :body-style="{ padding: '0px' }">
        <div>
          <img @click="onClickPeople(i)" :src="thumb" class="image"/>
          <div style="padding: 4px;">
            <span> {{i}} </span>
          </div>
        </div>
      </el-card>
    </div>
    </el-row>

    </el-main>
  </el-container>
</el-container>
</div>
</template>

<script>

import axios from 'axios'
import Vue from 'vue'

export default {
  name: 'Scan',
  data() {
    return {
      api_url: 'http://localhost:8080/cgi-bin/smartphoto',
      picture_server: 'http://localhost:8080/smartphoto/',
      upload_action_rul: 'http://localhost:8080/photo-upload/upload.py',

      reflushCount: 0,
      scaning: false,
      images: [],
      categories: {},
      people: {},
      labelClssPairs: {
        'person;individual;someone;somebody;mortal;soul': 'person',
        'sky' : 'sky',
        'tree': 'tree',
        'building;edifice': 'building',
        'plant;flora;plant;life': 'plant',
        'mountain;mount': 'mountain',
        'sea': 'sea',
        'animal;animate;being;beast;brute;creature;fauna': 'animal',
      },
      uploadFileList: [],
    }
  },

  mounted() {
    console.log('mounted')
    this.listClass()
    this.listPerson()
  },

  methods: {
    uploadRemove(file, fileList) {
      console.log(file, fileList)
    },

    uploadPreview(file) {
      console.log(file)
    },

    labelToClss(label) {
        if (this.labelClssPairs[label] == undefined)
          return null
        return this.labelClssPairs[label]
    },

    clssToLabel(clss) {
        for (const [key, value] of Object.entries(this.labelClssPairs)) {
            console.log(`${key}: ${value}`);
            if (value == clss)
              return key
        }
        return null
    },

    personFilter(person) {
      console.log('### person count:', person.count)
      if (person.count < 2)
        return null
      return person
    },

    reflushClassView() {
      console.log('do reflushClassView');
      this.listClass()
      this.listPerson()
      this.reflushCount += 1
      if (this.reflushCount < 10) {
        setTimeout(this.reflushClassView, 3000)
      }
      if (this.reflushCount > 1) {
        this.scaning = false;
      }
    },

    onScanStart() {
      let vc = this
      axios.post(this.api_url, {
        method: 'scan_start',
      }).then(function (response) {
        vc.scaning = true
        vc.reflushCount = 0
        vc.categories = {}
        vc.people = {}
        console.log('smartphoto response:', response)
        setTimeout(vc.reflushClassView, 3000)
      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    },

    onAllPhotos() {
      console.log('onAllPhotos')
      this.$router.push('/photo/all')
    },

    listClass() {
      let vc = this
      axios.post(this.api_url, {
        method: 'list_class',
      }).then(function (response) {
        console.log('smartphoto response:', response)
        if (response.data.result == 0) {
          for (const c of response.data.clsses) {
            let clss = vc.labelToClss(c.name)
            if (clss == null)
              continue
            if (vc.categories[clss] == undefined)
              vc.getClssThumbnailByLabel(c.name, clss)
          }
          console.log('categories=', vc.categories)
        }
      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    },

    listPerson() {
      let vc = this
      axios.post(this.api_url, {
        method: 'list_person',
      }).then(function (response) {
        console.log('smartphoto response:', response)
        if (response.data.result != 0) {
          return
        }
        for (const p of response.data.person) {
          const pp = vc.personFilter(p)
          if (pp == null)
            continue
          if (vc.people[pp.id] == undefined) {
            console.log('pp.id=', pp.id)
            vc.getPersonProfile(pp.id)
          }
        }
      }).catch(function (error) {
        console.log('smartphoto error:', error)
      })
    },

    getClssThumbnailByLabel(label, clss) {
      let vc = this

      axios.post(this.api_url, {
        method: 'list_photo_by_class',
        param: label,
      }).then(function (response) {
        console.log('### list_photo_by_class response:', response)

        /*let s = vc.picture_server + '/photos/'*/
        const t = vc.picture_server + '/thumbnail/crop_'

        for (let p of response.data.photos) {
          let filename = p.path.replace(/^.*(\\|\/)/, '')
          Vue.set(vc.categories, clss, t + filename)
        }
      }).catch(function (error) {
        console.log('list_photo_by_class error:', error)
      })
    },

    getPersonProfile(person) {
      let vc = this
      axios.post(this.api_url, {
        method: 'list_photo_by_person',
        param: person,
      }).then(function (response) {
        console.log('### list_photo_by_person response:', response)
        const t = vc.picture_server + '/thumbnail/crop_'

        for (let p of response.data.photos) {
          let filename = p.path.replace(/^.*(\\|\/)/, '')
          console.log('### list_photo_by_person filename:', filename)
          Vue.set(vc.people, person, t + filename)
        }
      }).catch(function (error) {
        console.log('list_photo_by_person error:', error)
      })
    },

    onClickCategory(clss) {
      console.log('onClickCategory clss:', clss)
      const label = this.clssToLabel(clss)
      console.log('onClickCategory label:', label)
      this.$router.push('/photo/categories/'+label)
    },

    onClickPeople(i) {
      console.log('onClickPeople:', i)
      this.$router.push('/photo/people/'+i)
    },
  },
}

</script>

<style scoped>

</style>

