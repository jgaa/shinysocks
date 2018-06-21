#!/usr/bin/env groovy

pipeline {
    agent { label 'master' }

    stages {
        stage ('Build') {
            steps {
                sh 'pwd'
                sh 'ls -la'
                sh 'rm -rf build'
                sh 'mkdir build'
                sh 'cd build && cmake -DCMAKE_BUILD_TYPE=Release .. && make'
            }
        }

        stage('Build Container') {

            steps {
                sh "docker build -t jgaafromnorth/shinysocks:v${env.BUILD_ID} -f ci/Dockerfile ."
                sh "docker tag jgaafromnorth/shinysocks:v${env.BUILD_ID} jgaafromnorth/shinysocks:latest"
            }
        }

        stage('Test Container') {

            steps {
                sh "docker run -d --rm -p 1080:1080 --name shinysocks-test jgaafromnorth/shinysocks:v${env.BUILD_ID}"
                sh "timeout 5 curl -x socks5://localhost:1080 https://google.com/"
            }
        }

        stage('Push to Docker Hub') {
            steps {
                withDockerRegistry([ credentialsId: "8cb91394-2af2-4477-8db8-8c0e13605900", url: "" ]) {
                    sh "docker push jgaafromnorth/shinysocks:v${env.BUILD_ID}"
                    sh 'docker push jgaafromnorth/shinysocks:latest'
                }
            }
        }
    }

    post {
        always {
            deleteDir()
            sh "docker stop shinysocks-test"
        }
    }
}
