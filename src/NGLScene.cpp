#include <QMouseEvent>
#include <QGuiApplication>

#include "NGLScene.h"
#include <ngl/Camera.h>
#include <ngl/Light.h>
#include <ngl/Material.h>
#include <ngl/NGLInit.h>
#include <ngl/VAOPrimitives.h>
#include <ngl/ShaderLib.h>
#include <stdio.h>


static int closestNeighbourID=-1;
static bool stopFalling=false;


//----------------------------------------------------------------------------------------------------------------------
/// @brief the increment for x/y translation with mouse movement
//----------------------------------------------------------------------------------------------------------------------
const static float INCREMENT=0.01f;
//----------------------------------------------------------------------------------------------------------------------
/// @brief the increment for the wheel zoom
//----------------------------------------------------------------------------------------------------------------------
const static float ZOOM=0.1f;

NGLScene::NGLScene()
{
  // re-size the widget to that of the parent (in that case the GLFrame passed in on construction)
  m_rotate=false;
  // mouse rotation values set to 0
  m_spinXFace=0.0f;
  m_spinYFace=0.0f;
  setTitle("Qt5 Simple NGL Demo");
}


NGLScene::~NGLScene()
{
  std::cout<<"Shutting down NGL, removing VAO's and Shaders\n";
}

void NGLScene::resizeGL(QResizeEvent *_event)
{
  m_width=static_cast<int>(_event->size().width()*devicePixelRatio());
  m_height=static_cast<int>(_event->size().height()*devicePixelRatio());
  // now set the camera size values as the screen size has changed
  m_cam.setShape(45.0f,static_cast<float>(width())/height(),0.05f,350.0f);
}

void NGLScene::resizeGL(int _w , int _h)
{
  m_cam.setShape(45.0f,static_cast<float>(_w)/_h,0.05f,350.0f);
  m_width=static_cast<int>(_w*devicePixelRatio());
  m_height=static_cast<int>(_h*devicePixelRatio());
}


void NGLScene::initializeGL()
{
  // we must call that first before any other GL commands to load and link the
  // gl commands from the lib, if that is not done program will crash
  ngl::NGLInit::instance();
  glClearColor(0.4f, 0.4f, 0.4f, 1.0f);			   // Grey Background
  // enable depth testing for drawing
  glEnable(GL_DEPTH_TEST);
  // enable multisampling for smoother drawing
#ifndef USINGIOS_
  glEnable(GL_MULTISAMPLE);
#endif
   // now to load the shader and set the values
  // grab an instance of shader manager
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  // we are creating a shader called Phong to save typos
  // in the code create some constexpr
  constexpr auto shaderProgram="Phong";
  constexpr auto vertexShader="PhongVertex";
  constexpr auto fragShader="PhongFragment";
  // create the shader program
  shader->createShaderProgram(shaderProgram);
  // now we are going to create empty shaders for Frag and Vert
  shader->attachShader(vertexShader,ngl::ShaderType::VERTEX);
  shader->attachShader(fragShader,ngl::ShaderType::FRAGMENT);
  // attach the source
  shader->loadShaderSource(vertexShader,"shaders/PhongVertex.glsl");
  shader->loadShaderSource(fragShader,"shaders/PhongFragment.glsl");
  // compile the shaders
  shader->compileShader(vertexShader);
  shader->compileShader(fragShader);
  // add them to the program
  shader->attachShaderToProgram(shaderProgram,vertexShader);
  shader->attachShaderToProgram(shaderProgram,fragShader);


  // now we have associated that data we can link the shader
  shader->linkProgramObject(shaderProgram);
  // and make it active ready to load values
  (*shader)[shaderProgram]->use();
  // the shader will use the currently active material and light0 so set them
  ngl::Material m(ngl::STDMAT::GOLD);
  // load our material values to the shader into the structure material (see Vertex shader)
  m.loadToShader("material");
  // Now we will create a basic Camera from the graphics library
  // This is a static camera so it only needs to be set once
  // First create Values for the camera position
  ngl::Vec3 from(0,2,10);
  ngl::Vec3 to(0,0,0);
  ngl::Vec3 up(0,1,0);
  // now load to our new camera
  m_cam.set(from,to,up);
  // set the shape using FOV 45 Aspect Ratio based on Width and Height
  // The final two are near and far clipping planes of 0.5 and 10
  m_cam.setShape(45.0f,720.0f/576.0f,0.05f,350.0f);
  shader->setUniform("viewerPos",m_cam.getEye().toVec3());
  // now create our light that is done after the camera so we can pass the
  // transpose of the projection matrix to the light to do correct eye space
  // transformations
  ngl::Mat4 iv=m_cam.getViewMatrix();
  iv.transpose();
  ngl::Light light(ngl::Vec3(-2,5,2),ngl::Colour(1,1,1,1),ngl::Colour(1,1,1,1),ngl::LightModes::POINTLIGHT );
  light.setTransform(iv);
  // load these values to the shader as well
  light.loadToShader("light");

  corners.push_back(corner1);
  corners.push_back(corner2);
  corners.push_back(corner3);
  corners.push_back(corner4);

  startTimer(10);

}


void NGLScene::loadMatricesToShader()
{
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();

  ngl::Mat4 MV;
  ngl::Mat4 MVP;
  ngl::Mat3 normalMatrix;
  ngl::Mat4 M;
  M= m_trans.getMatrix()*m_mouseGlobalTX;
  MV=  M*m_cam.getViewMatrix();
  MVP= M*m_cam.getVPMatrix();
  normalMatrix=MV;
  normalMatrix.inverse();
  shader->setShaderParamFromMat4("MV",MV);
  shader->setShaderParamFromMat4("MVP",MVP);
  shader->setShaderParamFromMat3("normalMatrix",normalMatrix);
  shader->setShaderParamFromMat4("M",M);
}

void NGLScene::paintGL()
{
  glViewport(0,0,m_width,m_height);
  // clear the screen and depth buffer
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  // grab an instance of the shader manager
  ngl::ShaderLib *shader=ngl::ShaderLib::instance();
  (*shader)["Phong"]->use();

  // Rotation based on the mouse position for our global transform
  ngl::Mat4 rotX;
  ngl::Mat4 rotY;
  // create the rotation matrices
  rotX.rotateX(m_spinXFace);
  rotY.rotateY(m_spinYFace);
  // multiply the rotations
  m_mouseGlobalTX=rotY*rotX;
  // add the translations
  m_mouseGlobalTX.m_m[3][0] = m_modelPos.m_x;
  m_mouseGlobalTX.m_m[3][1] = m_modelPos.m_y;
  m_mouseGlobalTX.m_m[3][2] = m_modelPos.m_z;

   // get the VBO instance and draw the built in teapot
  ngl::VAOPrimitives *prim=ngl::VAOPrimitives::instance();

  // draw all 4 courner balls + dropping ball

  int neighbourID=0;
  for(auto &c: corners)
  {
      m_trans.setPosition(c);
      loadMatricesToShader();

      ngl::Material m(ngl::STDMAT::GOLD);
      ngl::Material m2(ngl::STDMAT::CHROME);

      //change the color to indicate that this neighbour is the closest to the dropping teapot
      if(neighbourID==closestNeighbourID)
      {
          // load our material values to the shader into the structure material (see Vertex shader)
          m2.loadToShader("material");

          //stop falling teapot from falling
          stopFalling=true;
      }
      else // load the gold shader
      {
          m.loadToShader("material");
      }

      prim->draw("teapot");


      neighbourID++;
  }

  m_trans.setPosition(droppingTeapot);
  loadMatricesToShader();
  prim->draw("teapot");




}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mouseMoveEvent (QMouseEvent * _event)
{
  // note the method buttons() is the button state when event was called
  // that is different from button() which is used to check which button was
  // pressed when the mousePress/Release event is generated
  if(m_rotate && _event->buttons() == Qt::LeftButton)
  {
    int diffx=_event->x()-m_origX;
    int diffy=_event->y()-m_origY;
    m_spinXFace += static_cast<int>( 0.5f * diffy);
    m_spinYFace += static_cast<int>( 0.5f * diffx);
    m_origX = _event->x();
    m_origY = _event->y();
    update();

  }
        // right mouse translate code
  else if(m_translate && _event->buttons() == Qt::RightButton)
  {
    int diffX = static_cast<int>(_event->x() - m_origXPos);
    int diffY = static_cast<int>(_event->y() - m_origYPos);
    m_origXPos=_event->x();
    m_origYPos=_event->y();
    m_modelPos.m_x += INCREMENT * diffX;
    m_modelPos.m_y -= INCREMENT * diffY;
    update();

   }
}



ngl::Vec3 NGLScene::getWorldSpace(int _x, int _y)
{
  std::cout<<"Mouse pos "<<_x<<" "<<_y<<" ";
  ngl::Mat4 t=m_cam.getProjectionMatrix();//m_projection;
  ngl::Mat4 v=m_cam.getViewMatrix();//m_view;
  // as ngl:: and OpenGL use different formats need to transpose the matrix.
  t.transpose();
  v.transpose();
  ngl::Mat4 inverse=( t*v).inverse();

  ngl::Vec4 tmp(0,0,1.0f,1.0f);
  // convert into NDC
  tmp.m_x=(2.0f * _x) / width() - 1.0f;
  tmp.m_y=1.0f - (2.0f * _y) / height();
  // scale by inverse MV * Project transform
  ngl::Vec4 obj=inverse*tmp;
  // Scale by w
  obj/=obj.m_w;

//  std::cout<<"Coordinates in object space:"<<obj.m_x<<","<<obj.m_y<<","<<obj.m_z<<std::endl;

  return obj.toVec3();
  /* ngl now has this built in as well
  return ngl::unProject(ngl::Vec3(_x,_y,1.0f),
                        m_view,
                        m_projection,
                        ngl::Vec4(0,0,width(),height())
                        );
*/
}



//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mousePressEvent ( QMouseEvent * _event)
{
  // that method is called when the mouse button is pressed in this case we
  // store the value where the maouse was clicked (x,y) and set the Rotate flag to true
  if(_event->button() == Qt::LeftButton)
  {

      //Works //      https://en.wikibooks.org/wiki/OpenGL_Programming/Object_selection
      //we need to compare the colour to something meaningful, so if we assign a colour to each object and then perform the following code we know which object we clicked
    m_origX = _event->x();
    m_origY = _event->y();
    m_rotate =true;

//    GLbyte color[4];
//    GLfloat depth;
//    GLuint index;

//    glReadPixels(_event->x(), m_height - _event->y() - 1, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, color);
//    glReadPixels(_event->x(), m_height - _event->y() - 1, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &depth);
//    glReadPixels(_event->x(), m_height - _event->y() - 1, 1, 1, GL_STENCIL_INDEX, GL_UNSIGNED_INT, &index);

//    std::cout<<"Clicked on pixel "<<_event->x()<<", "<<_event->y()<<"\ncolor "<<(int)color[0]<<","<<(int)color[1]<<","<<(int)color[2]<<","<<(int)color[3]<<"\ndepth "<<depth<<"\nstencil "<<index<<std::endl;;

      // 2nd way
      //Works, now we need to tell which of the objects x,y to mouse x,y distance is the least so as to tell which one we clicked. also we need to check if we clicked the background
      ngl::Vec3 objcoord(getWorldSpace(_event->x(), _event->y()));
      std::cout<<"Coordinates in object space:"<<objcoord.m_x<<","<<objcoord.m_y<<","<<objcoord.m_z<<std::endl;



  }
  // right mouse translate mode
  else if(_event->button() == Qt::RightButton)
  {
    m_origXPos = _event->x();
    m_origYPos = _event->y();
    m_translate=true;
  }

}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::mouseReleaseEvent ( QMouseEvent * _event )
{
  // that event is called when the mouse button is released
  // we then set Rotate to false
  if (_event->button() == Qt::LeftButton)
  {
    m_rotate=false;
  }
        // right mouse translate mode
  if (_event->button() == Qt::RightButton)
  {
    m_translate=false;
  }
}

//----------------------------------------------------------------------------------------------------------------------
void NGLScene::wheelEvent(QWheelEvent *_event)
{

	// check the diff of the wheel position (0 means no change)
	if(_event->delta() > 0)
	{
		m_modelPos.m_z+=ZOOM;
	}
	else if(_event->delta() <0 )
	{
		m_modelPos.m_z-=ZOOM;
	}
	update();
}
//----------------------------------------------------------------------------------------------------------------------

void NGLScene::keyPressEvent(QKeyEvent *_event)
{
  // that method is called every time the main window recives a key event.
  // we then switch on the key value and set the camera in the GLWindow
  switch (_event->key())
  {
  // escape key to quit
  case Qt::Key_Escape : QGuiApplication::exit(EXIT_SUCCESS); break;
  // turn on wirframe rendering
#ifndef USINGIOS_

  case Qt::Key_W : glPolygonMode(GL_FRONT_AND_BACK,GL_LINE); break;
  // turn off wire frame
  case Qt::Key_S : glPolygonMode(GL_FRONT_AND_BACK,GL_FILL); break;
#endif
  // show full screen
  case Qt::Key_F : showFullScreen(); break;
  // show windowed
  case Qt::Key_N : showNormal(); break;
  default : break;
  }
 update();
}


void NGLScene::timerEvent(QTimerEvent *)
{
    if(!stopFalling)
    {
        droppingTeapot.m_y-=0.01;

        if (droppingTeapot.m_y <= 0.0)//at the surface point perform check with all neighbours
        {
            float minLength=1000;//intentionally large number
            float currentLength=0;
            int neighbourID=0;

            for(const auto &c: corners)
            {
                currentLength=(c-droppingTeapot).lengthSquared();
                if (currentLength < minLength)
                {
                    minLength = currentLength;
                    //so save this neighbours ID for now
                    closestNeighbourID=neighbourID;
                }

                neighbourID++;

            }
        }
    }

//  static float time;
//  time+=0.1f;
//  GLuint id=m_vao->getBufferID();
//  m_vao->bind();
//  ngl::Vec3   *ptr = static_cast<ngl::Vec3 *>(glMapBuffer(GL_ARRAY_BUFFER, GL_READ_WRITE));

//  std::cout<<"ptr[5]="<<ptr[0]<<std::endl;

//  for(int i=0; i<m_nVerts; ++i)
//    ptr[i].m_y=sinf(ptr[i].m_x+time);
//  for(int i=0; i<m_nVerts; ++i)
//    ptr[i].m_y+=sinf(ptr[i].m_z+time);

//  glUnmapBuffer(GL_ARRAY_BUFFER); // unmap it after use

//  m_vao->unbind();
  update();
}


