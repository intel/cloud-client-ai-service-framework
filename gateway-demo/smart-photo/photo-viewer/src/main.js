import Vue from 'vue'
import App from './App.vue'
import 'element-ui/lib/theme-chalk/index.css';

Vue.config.productionTip = false


import {
	Tree,
	Divider,
	Container,
	Header,
	Aside,
	Main,
	Row,
	Col,
	Button,
	Card,
	Form,
	FormItem,
	Input,
	Dialog,
	Menu,
	MenuItem,
	Submenu,
	Table,
	TableColumn,
	Drawer,
	Link,
	InputNumber,
	Collapse,
	CollapseItem,
	DatePicker,
	Tag,
} from 'element-ui'

Vue.use(Tree)
Vue.use(Divider)
Vue.use(Container)
Vue.use(Header)
Vue.use(Aside)
Vue.use(Main)
Vue.use(Row)
Vue.use(Col)
Vue.use(Button)
Vue.use(Card)
Vue.use(Form)
Vue.use(FormItem)
Vue.use(Input)
Vue.use(Dialog)
Vue.use(Menu)
Vue.use(MenuItem)
Vue.use(Submenu)
Vue.use(Table)
Vue.use(TableColumn)
Vue.use(Drawer)
Vue.use(Link)
Vue.use(InputNumber)
Vue.use(Collapse)
Vue.use(CollapseItem)
Vue.use(DatePicker)
Vue.use(Tag)


import 'viewerjs/dist/viewer.css'
import Viewer from 'v-viewer'
Vue.use(Viewer)
Viewer.setDefaults({ url: "data-src", })

new Vue({
  render: h => h(App),
}).$mount('#app')
